package main

// Soundpad Remote Server (Windows, x64).
// HTTP server that injects key combos via SendInput.
//
// Two build flavors (same source):
//   server.exe       console app  -> logs to console + server.log
//   server-tray.exe  GUI app      -> no console, tray icon, logs to server.log
//     build: go build -ldflags "-H=windowsgui" -o server-tray.exe .

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"time"
	"unsafe"

	"fyne.io/systray"
)

// ---------------------------------------------------------------------------
// Win32 key injection (SendInput via syscall)
// ---------------------------------------------------------------------------

var (
	user32               = syscall.NewLazyDLL("user32.dll")
	procSendInput        = user32.NewProc("SendInput")
	procGetAsyncKeyState = user32.NewProc("GetAsyncKeyState")
	procGetKeyState      = user32.NewProc("GetKeyState")
)

const (
	inputKeyboard     = 1
	keyeventfExtended = 0x0001
	keyeventfKeyUp    = 0x0002
)

// keybdInput matches the Win32 KEYBDINPUT structure.
type keybdInput struct {
	wVk         uint16
	wScan       uint16
	dwFlags     uint32
	time        uint32
	dwExtraInfo uintptr
}

// input matches the Win32 INPUT structure (union flattened; keyboard part only).
// sizeof must be 40 bytes on amd64.
type input struct {
	typ   uint32
	_pad  uint32
	ki    keybdInput
	_tail uint64
}

type keyEvent struct {
	vk       uint16
	up       bool
	extended bool
}

func sendEvents(events []keyEvent) error {
	if len(events) == 0 {
		return nil
	}
	inputs := make([]input, len(events))
	for i, e := range events {
		var flags uint32
		if e.up {
			flags |= keyeventfKeyUp
		}
		if e.extended {
			flags |= keyeventfExtended
		}
		inputs[i] = input{typ: inputKeyboard, ki: keybdInput{wVk: e.vk, dwFlags: flags}}
	}
	cb := int32(unsafe.Sizeof(input{}))
	ret, _, callErr := procSendInput.Call(
		uintptr(len(inputs)),
		uintptr(unsafe.Pointer(&inputs[0])),
		uintptr(cb),
	)
	if ret != uintptr(len(inputs)) {
		return fmt.Errorf("SendInput sent %d of %d (err=%v)", ret, len(inputs), callErr)
	}
	return nil
}

// ---------------------------------------------------------------------------
// Virtual-key map and combo parsing
// ---------------------------------------------------------------------------

var vkMap = buildVkMap()

func buildVkMap() map[string]uint16 {
	m := map[string]uint16{
		"backspace": 0x08, "tab": 0x09, "enter": 0x0D, "return": 0x0D,
		"shift": 0x10, "ctrl": 0x11, "control": 0x11, "alt": 0x12, "menu": 0x12,
		"pause": 0x13, "capslock": 0x14,
		"esc": 0x1B, "escape": 0x1B,
		"space": 0x20, "spacebar": 0x20,
		"pageup": 0x21, "pagedown": 0x22, "end": 0x23, "home": 0x24,
		"left": 0x25, "up": 0x26, "right": 0x27, "down": 0x28,
		"printscreen": 0x2C, "insert": 0x2D, "delete": 0x2E, "del": 0x2E,
		"win": 0x5B, "lwin": 0x5B, "rwin": 0x5C, "windows": 0x5B, "super": 0x5B, "meta": 0x5B,
		"multiply": 0x6A, "add": 0x6B, "separator": 0x6C, "subtract": 0x6D, "decimal": 0x6E, "divide": 0x6F,
		"numlock": 0x90, "scrolllock": 0x91,
		"volume_mute": 0xAD, "volume_down": 0xAE, "volume_up": 0xAF,
		"media_next": 0xB0, "media_prev": 0xB1, "media_stop": 0xB2, "media_play_pause": 0xB3,
	}
	for i := 0; i <= 9; i++ {
		m[fmt.Sprintf("numpad%d", i)] = 0x60 + uint16(i)
	}
	for i := 1; i <= 24; i++ {
		m[fmt.Sprintf("f%d", i)] = 0x70 + uint16(i-1)
	}
	return m
}

var singleCharVk = map[string]uint16{
	";": 0xBA, "=": 0xBB, ",": 0xBC, "-": 0xBD, ".": 0xBE, "/": 0xBF,
	"`": 0xC0, "[": 0xDB, "\\": 0xDC, "]": 0xDD, "'": 0xDE,
}

var extendedKeys = map[string]bool{
	"left": true, "up": true, "right": true, "down": true,
	"insert": true, "delete": true, "del": true, "home": true, "end": true,
	"pageup": true, "pagedown": true, "printscreen": true,
	"win": true, "lwin": true, "rwin": true, "windows": true, "super": true, "meta": true,
	"divide": true,
	"volume_mute": true, "volume_down": true, "volume_up": true,
	"media_next": true, "media_prev": true, "media_stop": true, "media_play_pause": true,
}

var (
	ctrlNames  = map[string]bool{"ctrl": true, "control": true}
	altNames   = map[string]bool{"alt": true, "menu": true}
	shiftNames = map[string]bool{"shift": true}
	winNames   = map[string]bool{"win": true, "windows": true, "super": true, "meta": true, "lwin": true, "rwin": true}
)

func resolveVk(name string) (uint16, error) {
	lower := strings.ToLower(name)
	if len(lower) == 1 {
		c := lower[0]
		if c >= 'a' && c <= 'z' {
			return uint16(c-'a') + 0x41, nil
		}
		if c >= '0' && c <= '9' {
			return uint16(c), nil
		}
		if v, ok := singleCharVk[lower]; ok {
			return v, nil
		}
		return 0, fmt.Errorf("unknown key: %s", name)
	}
	if v, ok := vkMap[lower]; ok {
		return v, nil
	}
	return 0, fmt.Errorf("unknown key: %s", name)
}

// expandNumAlias turns the convenience names num10..num29 into real combos.
//
//	num10..num19 -> numpad0..numpad9
//	num20..num29 -> ctrl+numpad0..ctrl+numpad9
//
// Numpad keys need NumLock on; set "numlock": true in config.json and the
// server turns it on automatically before sending such a combo.
func expandNumAlias(combo string) string {
	c := strings.ToLower(strings.TrimSpace(combo))
	if !strings.HasPrefix(c, "num") {
		return combo
	}
	n, err := strconv.Atoi(strings.TrimPrefix(c, "num"))
	if err != nil {
		return combo
	}
	switch {
	case n >= 10 && n <= 19:
		return fmt.Sprintf("numpad%d", n-10)
	case n >= 20 && n <= 29:
		return fmt.Sprintf("ctrl+numpad%d", n-20)
	}
	return combo
}

const vkNumLock = 0x90

// numLockOn reads the NumLock toggle state. GetKeyState (unlike
// GetKeyboardState) reflects the current system-wide state for toggle keys.
func numLockOn() bool {
	r, _, _ := procGetKeyState.Call(vkNumLock)
	return uint16(r)&0x0001 != 0
}

// ensureNumLock turns NumLock on if it is off. Without it numpad keys behave
// as arrows/Home/End for games that read scancodes.
func ensureNumLock() {
	if numLockOn() {
		return
	}
	_ = sendEvents([]keyEvent{{vk: vkNumLock}, {vk: vkNumLock, up: true}})
	time.Sleep(10 * time.Millisecond)
}

func injectCombo(combo string) error {
	combo = expandNumAlias(combo)
	if forceNumLock && strings.Contains(strings.ToLower(combo), "numpad") {
		ensureNumLock()
	}

	var toks []string
	for _, p := range strings.Split(combo, "+") {
		t := strings.ToLower(strings.TrimSpace(p))
		if t != "" {
			toks = append(toks, t)
		}
	}

	var ctrl, alt, shift, win bool
	var mainName string
	for _, t := range toks {
		switch {
		case ctrlNames[t]:
			ctrl = true
		case altNames[t]:
			alt = true
		case shiftNames[t]:
			shift = true
		case winNames[t]:
			win = true
		default:
			if mainName == "" {
				mainName = t
			} else {
				return fmt.Errorf("more than one main key in combo: %s", combo)
			}
		}
	}
	if mainName == "" {
		return fmt.Errorf("no main key in combo: %s", combo)
	}

	mainVk, err := resolveVk(mainName)
	if err != nil {
		return err
	}
	mainExt := extendedKeys[mainName]

	var events []keyEvent
	if ctrl {
		events = append(events, keyEvent{vk: vkMap["ctrl"]})
	}
	if alt {
		events = append(events, keyEvent{vk: vkMap["alt"]})
	}
	if shift {
		events = append(events, keyEvent{vk: vkMap["shift"]})
	}
	if win {
		events = append(events, keyEvent{vk: vkMap["win"], extended: true})
	}
	events = append(events, keyEvent{vk: mainVk, extended: mainExt})
	events = append(events, keyEvent{vk: mainVk, up: true, extended: mainExt})
	if win {
		events = append(events, keyEvent{vk: vkMap["win"], up: true, extended: true})
	}
	if shift {
		events = append(events, keyEvent{vk: vkMap["shift"], up: true})
	}
	if alt {
		events = append(events, keyEvent{vk: vkMap["alt"], up: true})
	}
	if ctrl {
		events = append(events, keyEvent{vk: vkMap["ctrl"], up: true})
	}

	return sendEvents(events)
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

var (
	logger     = log.New(io.Discard, "", log.LstdFlags)
	logFile    *os.File
	httpServer *http.Server

	// forceNumLock mirrors config.json "numlock": turn NumLock on before
	// sending a numpad combo.
	forceNumLock bool

	// trayMode is set at build time for the tray flavor:
	//   go build -ldflags "-H=windowsgui -X main.trayMode=true" -o server-tray.exe .
	trayMode = "false"
)

func setupLogging(toStdout bool) {
	path := filepath.Join(exeDir(), "server.log")
	f, err := os.OpenFile(path, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o644)
	if err != nil {
		f = nil
	}
	logFile = f

	var w io.Writer = io.Discard
	switch {
	case toStdout && f != nil:
		w = io.MultiWriter(os.Stdout, f)
	case toStdout:
		w = os.Stdout
	case f != nil:
		w = f
	}
	logger = log.New(w, "", log.LstdFlags)
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

type keyDef struct {
	Label string `json:"label"`
	Combo string `json:"combo"`
	// Order controls the position in /keys (lower first). Optional; ties fall
	// back to a natural sort of the id, so sound2 comes before sound10.
	Order int `json:"order"`
}

type config struct {
	Host    string            `json:"host"`
	Port    int               `json:"port"`
	Token   string            `json:"token"`
	NumLock bool              `json:"numlock"`
	Keys    map[string]keyDef `json:"keys"`
}

func exeDir() string {
	exe, err := os.Executable()
	if err != nil {
		return "."
	}
	return filepath.Dir(exe)
}

func loadConfig() (*config, error) {
	p := filepath.Join(exeDir(), "config.json")
	b, err := os.ReadFile(p)
	if err != nil {
		return nil, fmt.Errorf("read %s: %w", p, err)
	}
	var c config
	if err := json.Unmarshal(b, &c); err != nil {
		return nil, fmt.Errorf("parse config: %w", err)
	}
	if c.Host == "" {
		c.Host = "0.0.0.0"
	}
	if c.Port == 0 {
		c.Port = 8080
	}
	if c.Keys == nil {
		c.Keys = map[string]keyDef{}
	}
	forceNumLock = c.NumLock
	return &c, nil
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------

func writeJSON(w http.ResponseWriter, code int, v interface{}) {
	b, _ := json.Marshal(v)
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.WriteHeader(code)
	_, _ = w.Write(b)
}

func clientIP(r *http.Request) string {
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		return r.RemoteAddr
	}
	return host
}

func authorized(c *config, r *http.Request) bool {
	if c.Token == "" {
		return true
	}
	if r.Header.Get("X-Key") == c.Token {
		return true
	}
	if r.URL.Query().Get("key") == c.Token {
		return true
	}
	return false
}

func isDigit(b byte) bool { return b >= '0' && b <= '9' }

// naturalLess orders ids like sound2 before sound10 (plain string sort puts
// sound10 first because '1' < '2').
func naturalLess(a, b string) bool {
	ia, ib := 0, 0
	for ia < len(a) && ib < len(b) {
		if isDigit(a[ia]) && isDigit(b[ib]) {
			ja, jb := ia, ib
			for ja < len(a) && isDigit(a[ja]) {
				ja++
			}
			for jb < len(b) && isDigit(b[jb]) {
				jb++
			}
			na := strings.TrimLeft(a[ia:ja], "0")
			nb := strings.TrimLeft(b[ib:jb], "0")
			if len(na) != len(nb) {
				return len(na) < len(nb)
			}
			if na != nb {
				return na < nb
			}
			ia, ib = ja, jb
			continue
		}
		if a[ia] != b[ib] {
			return a[ia] < b[ib]
		}
		ia++
		ib++
	}
	return len(a)-ia < len(b)-ib
}

func handleKeys(c *config, w http.ResponseWriter, r *http.Request) {
	ip := clientIP(r)
	if !authorized(c, r) {
		logger.Printf("unauthorized %s %s from=%s", r.Method, r.URL.Path, ip)
		writeJSON(w, 401, map[string]string{"error": "unauthorized"})
		return
	}
	ids := make([]string, 0, len(c.Keys))
	for id := range c.Keys {
		ids = append(ids, id)
	}
	sort.SliceStable(ids, func(i, j int) bool {
		oi, oj := c.Keys[ids[i]].Order, c.Keys[ids[j]].Order
		if oi != oj {
			return oi < oj
		}
		return naturalLess(ids[i], ids[j])
	})

	type keyOut struct {
		ID    string `json:"id"`
		Label string `json:"label"`
		Combo string `json:"combo"`
	}
	list := make([]keyOut, 0, len(ids))
	for _, id := range ids {
		k := c.Keys[id]
		label := k.Label
		if label == "" {
			label = id
		}
		list = append(list, keyOut{ID: id, Label: label, Combo: k.Combo})
	}
	logger.Printf("list keys from=%s (%d)", ip, len(list))
	writeJSON(w, 200, map[string]interface{}{"keys": list})
}

func handlePress(c *config, w http.ResponseWriter, r *http.Request) {
	ip := clientIP(r)
	if !authorized(c, r) {
		logger.Printf("unauthorized press from=%s", ip)
		writeJSON(w, 401, map[string]string{"error": "unauthorized"})
		return
	}

	id := r.URL.Query().Get("id")
	combo := r.URL.Query().Get("combo")

	if r.Method == http.MethodPost {
		var body struct {
			ID    string `json:"id"`
			Combo string `json:"combo"`
		}
		b, _ := io.ReadAll(io.LimitReader(r.Body, 1<<20))
		if len(b) > 0 {
			_ = json.Unmarshal(b, &body)
			if body.ID != "" {
				id = body.ID
			}
			if body.Combo != "" {
				combo = body.Combo
			}
		}
	}

	if id != "" {
		k, ok := c.Keys[id]
		if !ok {
			logger.Printf("press unknown id=%s from=%s", id, ip)
			writeJSON(w, 404, map[string]string{"error": "unknown id: " + id})
			return
		}
		combo = k.Combo
	}

	if combo == "" {
		writeJSON(w, 400, map[string]string{"error": "provide ?id=<id> or ?combo=<combo>"})
		return
	}

	if err := injectCombo(combo); err != nil {
		logger.Printf("press error combo=%s from=%s: %v", combo, ip, err)
		writeJSON(w, 500, map[string]string{"error": err.Error()})
		return
	}
	logger.Printf("press combo=%s from=%s", combo, ip)
	writeJSON(w, 200, map[string]interface{}{"ok": true, "combo": combo})
}

func serveIndex(w http.ResponseWriter, r *http.Request) {
	p := filepath.Join(exeDir(), "public", "index.html")
	b, err := os.ReadFile(p)
	if err != nil {
		http.Error(w, "index.html not found", http.StatusNotFound)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	_, _ = w.Write(b)
}

func newHandler(c *config) http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/", "/index.html":
			serveIndex(w, r)
		case "/keys":
			handleKeys(c, w, r)
		case "/press":
			handlePress(c, w, r)
		default:
			writeJSON(w, 404, map[string]string{"error": "not found"})
		}
	})
	return mux
}

func startServer(c *config) {
	addr := fmt.Sprintf("%s:%d", c.Host, c.Port)
	httpServer = &http.Server{Addr: addr, Handler: newHandler(c)}
	logger.Printf("server started addr=%s keys=%d token=%t", addr, len(c.Keys), c.Token != "")
	if err := httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		logger.Printf("server error: %v", err)
	}
}

// ---------------------------------------------------------------------------
// Tray
// ---------------------------------------------------------------------------

func buildIcon() []byte {
	const size = 16
	// XOR bitmap (BGRA, bottom-up), fully opaque solid square.
	xor := make([]byte, size*size*4)
	for y := 0; y < size; y++ {
		for x := 0; x < size; x++ {
			oy := size - 1 - y
			off := (oy*size + x) * 4
			xor[off+0] = 0xB9 // B
			xor[off+1] = 0x80 // G
			xor[off+2] = 0x29 // R
			xor[off+3] = 0xFF // A
		}
	}
	and := make([]byte, size*((size+31)/32)*4) // 64 bytes, all 0

	imgSize := 40 + len(xor) + len(and)
	var buf bytes.Buffer
	// ICONDIR
	_ = binary.Write(&buf, binary.LittleEndian, uint16(0)) // reserved
	_ = binary.Write(&buf, binary.LittleEndian, uint16(1)) // type: icon
	_ = binary.Write(&buf, binary.LittleEndian, uint16(1)) // count
	// ICONDIRENTRY
	buf.WriteByte(size)                                    // width
	buf.WriteByte(size)                                    // height
	buf.WriteByte(0)                                       // palette
	buf.WriteByte(0)                                       // reserved
	_ = binary.Write(&buf, binary.LittleEndian, uint16(1))  // planes
	_ = binary.Write(&buf, binary.LittleEndian, uint16(32)) // bpp
	_ = binary.Write(&buf, binary.LittleEndian, uint32(imgSize))
	_ = binary.Write(&buf, binary.LittleEndian, uint32(22)) // offset
	// BITMAPINFOHEADER
	_ = binary.Write(&buf, binary.LittleEndian, uint32(40))    // hdr size
	_ = binary.Write(&buf, binary.LittleEndian, int32(size))   // width
	_ = binary.Write(&buf, binary.LittleEndian, int32(size*2)) // height (xor+and)
	_ = binary.Write(&buf, binary.LittleEndian, uint16(1))     // planes
	_ = binary.Write(&buf, binary.LittleEndian, uint16(32))    // bpp
	_ = binary.Write(&buf, binary.LittleEndian, uint32(0))     // compression
	_ = binary.Write(&buf, binary.LittleEndian, uint32(0))     // sizeImage
	_ = binary.Write(&buf, binary.LittleEndian, int32(0))      // xppm
	_ = binary.Write(&buf, binary.LittleEndian, int32(0))      // yppm
	_ = binary.Write(&buf, binary.LittleEndian, uint32(0))     // clrUsed
	_ = binary.Write(&buf, binary.LittleEndian, uint32(0))     // clrImportant
	buf.Write(xor)
	buf.Write(and)
	return buf.Bytes()
}

func openBrowser(url string) {
	_ = exec.Command("rundll32", "url.dll,FileProtocolHandler", url).Start()
}

func runTray() {
	c, err := loadConfig()
	if err != nil {
		logger.Fatalf("config error: %v", err)
	}

	go startServer(c)

	url := fmt.Sprintf("http://localhost:%d", c.Port)
	logPath := filepath.Join(exeDir(), "server.log")

	systray.Run(func() {
		systray.SetTitle("HID Emulator")
		systray.SetTooltip(fmt.Sprintf("HID Emulator :%d", c.Port))
		systray.SetIcon(buildIcon())

		mWeb := systray.AddMenuItem("Открыть веб-интерфейс", url)
		mLog := systray.AddMenuItem("Открыть лог", logPath)
		systray.AddSeparator()
		mQuit := systray.AddMenuItem("Выход", "Остановить сервер")

		for {
			select {
			case <-mWeb.ClickedCh:
				openBrowser(url)
			case <-mLog.ClickedCh:
				_ = exec.Command("notepad.exe", logPath).Start()
			case <-mQuit.ClickedCh:
				logger.Printf("exit requested from tray")
				systray.Quit()
				return
			}
		}
	}, func() {
		logger.Printf("server stopped")
		if httpServer != nil {
			_ = httpServer.Close()
		}
	})
}

// ---------------------------------------------------------------------------
// Console mode
// ---------------------------------------------------------------------------

func runConsole() {
	c, err := loadConfig()
	if err != nil {
		logger.Fatalf("config error: %v", err)
	}
	startServer(c) // blocks
}

// ---------------------------------------------------------------------------
// Self-test
// ---------------------------------------------------------------------------

func runSelfTest() {
	logger.Printf("sizeof(INPUT)=%d (expect 40 on amd64)", unsafe.Sizeof(input{}))
	logger.Printf("running key-injection self-test (F13 should be seen by the OS)...")

	const f13 = 0x7C
	if err := sendEvents([]keyEvent{{vk: f13}}); err != nil {
		logger.Printf("SELF-TEST FAILED: %v", err)
		os.Exit(1)
	}

	down := false
	deadline := time.Now().Add(200 * time.Millisecond)
	for time.Now().Before(deadline) {
		if r, _, _ := procGetAsyncKeyState.Call(uintptr(f13)); r&0x8000 != 0 {
			down = true
			break
		}
		time.Sleep(5 * time.Millisecond)
	}

	_ = sendEvents([]keyEvent{{vk: f13, up: true}})

	if down {
		logger.Printf("SELF-TEST PASSED")
		os.Exit(0)
	}
	logger.Printf("SELF-TEST FAILED: F13 not seen by the OS")
	os.Exit(1)
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

func main() {
	mode := "console"
	for _, a := range os.Args[1:] {
		if a == "--test" {
			mode = "test"
		}
		if a == "--tray" {
			mode = "tray"
		}
	}
	if trayMode == "true" {
		mode = "tray"
	}

	setupLogging(mode != "tray") // stdout only in console/test mode
	defer func() {
		if logFile != nil {
			_ = logFile.Close()
		}
	}()

	switch mode {
	case "test":
		runSelfTest()
	case "tray":
		runTray()
	default:
		runConsole()
	}
}
