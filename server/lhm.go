package main

// Starts LibreHardwareMonitor together with the server and closes it on exit.
//
// The server only stops the instance it started itself. If LibreHardwareMonitor
// was already running before the server, it is left alone.
//
// "Already running" is detected through LHM's own HTTP server (data.json), so
// no tasklist/nvidia-smi console windows are spawned.

import (
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

var (
	lhmMu    sync.Mutex
	lhmCmd   *exec.Cmd
	lhmOwned bool
)

func findLHM(c *config) string {
	base := exeDir()

	if c != nil && c.LHM != nil && strings.TrimSpace(c.LHM.Path) != "" {
		p := c.LHM.Path
		if !filepath.IsAbs(p) {
			p = filepath.Join(base, p)
		}
		if st, err := os.Stat(p); err == nil && !st.IsDir() {
			return p
		}
		logger.Printf("LHM: configured path not found: %s", p)
		return ""
	}

	candidates := []string{
		filepath.Join(base, "LibreHardwareMonitor", "LibreHardwareMonitor.exe"),
		filepath.Join(base, "LibreHardwareMonitor", "LibreHardwareMonitor", "LibreHardwareMonitor.exe"),
		filepath.Join(base, "LibreHardwareMonitor.exe"),
	}
	for _, p := range candidates {
		if st, err := os.Stat(p); err == nil && !st.IsDir() {
			return p
		}
	}
	return ""
}

func lhmRunning(port int) bool {
	client := http.Client{Timeout: 500 * time.Millisecond}
	resp, err := client.Get(fmt.Sprintf("http://127.0.0.1:%d/data.json", port))
	if err != nil {
		return false
	}
	resp.Body.Close()
	return resp.StatusCode == 200
}

func startLHM(c *config) {
	if !monitoringEnabled {
		logger.Printf("LHM: monitoring disabled")
		return
	}

	lhmMu.Lock()
	defer lhmMu.Unlock()

	if lhmCmd != nil {
		return
	}
	if c != nil && c.LHM != nil && c.LHM.AutoStart != nil && !*c.LHM.AutoStart {
		logger.Printf("LHM: auto_start disabled")
		return
	}

	port := 8085
	if c != nil && c.LHM != nil && c.LHM.HTTPPort > 0 {
		port = c.LHM.HTTPPort
	}

	p := findLHM(c)
	if p == "" {
		logger.Printf("LHM: executable not found")
		return
	}
	if lhmRunning(port) {
		logger.Printf("LHM: already running, leaving it alone")
		return
	}

	cmd := exec.Command(p)
	cmd.Dir = filepath.Dir(p)
	if err := cmd.Start(); err != nil {
		logger.Printf("LHM: start failed: %v (run server as administrator)", err)
		return
	}

	lhmCmd = cmd
	lhmOwned = true
	logger.Printf("LHM: started pid=%d", cmd.Process.Pid)

	go func(cc *exec.Cmd) {
		_ = cc.Wait()
		lhmMu.Lock()
		if lhmCmd == cc {
			lhmCmd = nil
			lhmOwned = false
		}
		lhmMu.Unlock()
	}(cmd)
}

func stopLHM() {
	lhmMu.Lock()
	defer lhmMu.Unlock()

	if lhmCmd == nil || !lhmOwned {
		return
	}
	if lhmCmd.Process != nil {
		_ = lhmCmd.Process.Kill()
		logger.Printf("LHM: stopped pid=%d", lhmCmd.Process.Pid)
	}
	lhmCmd = nil
	lhmOwned = false
}
