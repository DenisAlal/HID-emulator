package main

// PC metrics for the ESP32 "PC" page. Everything comes from LibreHardwareMonitor's
// HTTP data.json: CPU load/temp/freq, GPU load/temp/VRAM, RAM. No nvidia-smi,
// no tasklist and no child console processes.

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"
)

type cpuStats struct {
	Load float64  `json:"load"`
	Temp *float64 `json:"temp"`
	Freq *float64 `json:"freq"` // MHz
}

type gpuStats struct {
	Name      string   `json:"name"`
	Load      float64  `json:"load"`
	Temp      *float64 `json:"temp"`
	VRAMUsed  int      `json:"vram_used"`  // MB
	VRAMTotal int      `json:"vram_total"` // MB
}

type ramStats struct {
	Used    int     `json:"used"`  // MB
	Total   int     `json:"total"` // MB
	Percent float64 `json:"percent"`
}

type statsPayload struct {
	OK   bool      `json:"ok"`
	CPU  cpuStats  `json:"cpu"`
	GPU  *gpuStats `json:"gpu"`
	RAM  ramStats  `json:"ram"`
	Show statsShow `json:"show"`
	TS   int64     `json:"ts"`
}

type lhmNode struct {
	Text       string    `json:"Text"`
	Type       string    `json:"Type"`
	RawValue   string    `json:"RawValue"`
	SensorID   string    `json:"SensorId"`
	HardwareID string    `json:"HardwareId"`
	Children   []lhmNode `json:"Children"`
}

// parseNumber extracts the numeric part and the unit from strings like
// "59,5 °C", "3875,0 MHz", "12,5 GB", "16,8 %", "11134,0 MB".
func parseNumber(s string) (float64, string, bool) {
	s = strings.TrimSpace(s)
	if s == "" {
		return 0, "", false
	}

	low := strings.ToLower(s)
	unit := ""
	idx := -1
	for _, u := range []string{"mhz", "ghz", "khz", "gb", "mb", "kb", "%", "°c"} {
		if i := strings.Index(low, u); i >= 0 {
			idx = i
			unit = u
			break
		}
	}
	if idx >= 0 {
		s = s[:idx]
	}

	s = strings.ReplaceAll(s, " ", "")
	s = strings.ReplaceAll(s, "\u00A0", "")
	s = strings.ReplaceAll(s, ",", ".")

	v, err := strconv.ParseFloat(s, 64)
	if err != nil {
		return 0, "", false
	}
	return v, unit, true
}

func toMB(v float64, unit string) float64 {
	switch unit {
	case "gb":
		return v * 1024
	case "kb":
		return v / 1024
	}
	return v
}

func readLHMSnapshot(c *config) statsPayload {
	port := 8085
	if c != nil && c.LHM != nil && c.LHM.HTTPPort > 0 {
		port = c.LHM.HTTPPort
	}

	out := statsPayload{
		Show: c.statsShow(),
		TS:   time.Now().Unix(),
	}

	client := http.Client{Timeout: 2 * time.Second}
	resp, err := client.Get(fmt.Sprintf("http://127.0.0.1:%d/data.json", port))
	if err != nil {
		return out
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return out
	}

	var root lhmNode
	if err := json.NewDecoder(resp.Body).Decode(&root); err != nil {
		return out
	}

	bestTempScore, bestFreqScore := -1, -1
	bestTempVal, bestFreqVal := 0.0, 0.0
	gpu := &gpuStats{}
	gpuFound := false
	ramUsedGB, ramAvailGB, ramPct := 0.0, 0.0, 0.0
	ramLoadFound, ramUsedFound, ramAvailFound := false, false, false
	found := false

	var walk func(n *lhmNode, hardware string)
	walk = func(n *lhmNode, hardware string) {
		cur := hardware
		if n.HardwareID != "" {
			cur = n.Text
		}
		id := strings.ToLower(n.SensorID)
		name := strings.ToLower(n.Text)

		if strings.HasPrefix(id, "/amdcpu/") || strings.HasPrefix(id, "/intelcpu/") {
			switch n.Type {
			case "Load":
				if name == "cpu total" {
					if v, _, ok := parseNumber(n.RawValue); ok {
						out.CPU.Load = v
						found = true
					}
				}
			case "Temperature":
				if v, _, ok := parseNumber(n.RawValue); ok && v > 0 && v < 150 {
					score := 0
					if strings.Contains(name, "tctl") {
						score += 20
					} else if strings.Contains(name, "tdie") {
						score += 10
					} else if strings.Contains(name, "core") {
						score += 5
					}
					if score > bestTempScore || (score == bestTempScore && v > bestTempVal) {
						bestTempScore, bestTempVal = score, v
					}
				}
			case "Clock":
				if !strings.Contains(name, "effective") {
					if v, unit, ok := parseNumber(n.RawValue); ok {
						if unit == "ghz" {
							v *= 1000
						} else if unit == "khz" {
							v /= 1000
						}
						score := 0
						if strings.Contains(name, "average") {
							score += 10
						}
						if strings.Contains(name, "core") {
							score += 2
						}
						if score > bestFreqScore || (score == bestFreqScore && v > bestFreqVal) {
							bestFreqScore, bestFreqVal = score, v
						}
					}
				}
			}
		}

		if strings.HasPrefix(id, "/gpu-nvidia/") ||
			strings.HasPrefix(id, "/gpu-amd/") ||
			strings.HasPrefix(id, "/gpu-intel/") {
			switch n.Type {
			case "Load":
				if name == "gpu core" {
					if v, _, ok := parseNumber(n.RawValue); ok {
						gpu.Load = v
						gpuFound = true
					}
				}
			case "Temperature":
				if name == "gpu core" {
					if v, _, ok := parseNumber(n.RawValue); ok && v > 0 && v < 150 {
						vv := v
						gpu.Temp = &vv
						gpuFound = true
					}
				}
			case "SmallData", "Data":
				if name == "gpu memory used" {
					if v, unit, ok := parseNumber(n.RawValue); ok {
						gpu.VRAMUsed = int(toMB(v, unit) + 0.5)
						gpuFound = true
					}
				} else if name == "gpu memory total" {
					if v, unit, ok := parseNumber(n.RawValue); ok {
						gpu.VRAMTotal = int(toMB(v, unit) + 0.5)
						gpuFound = true
					}
				}
			}
			if cur != "" {
				gpu.Name = cur
			}
		}

		if strings.HasPrefix(id, "/ram/") {
			switch n.Type {
			case "Load":
				if name == "memory" {
					if v, _, ok := parseNumber(n.RawValue); ok {
						ramPct = v
						ramLoadFound = true
						found = true
					}
				}
			case "Data":
				if name == "memory used" {
					if v, _, ok := parseNumber(n.RawValue); ok {
						ramUsedGB = v
						ramUsedFound = true
						found = true
					}
				}
				if name == "memory available" {
					if v, _, ok := parseNumber(n.RawValue); ok {
						ramAvailGB = v
						ramAvailFound = true
						found = true
					}
				}
			}
		}

		for i := range n.Children {
			walk(&n.Children[i], cur)
		}
	}
	walk(&root, "")

	if bestTempScore >= 0 {
		v := bestTempVal
		out.CPU.Temp = &v
		found = true
	}
	if bestFreqScore >= 0 {
		v := bestFreqVal
		out.CPU.Freq = &v
		found = true
	}
	if gpuFound {
		out.GPU = gpu
		found = true
	}

	if ramUsedFound || ramLoadFound {
		if ramUsedFound && ramAvailFound {
			out.RAM.Used = int(ramUsedGB*1024 + 0.5)
			out.RAM.Total = int((ramUsedGB+ramAvailGB)*1024 + 0.5)
			out.RAM.Percent = ramUsedGB * 100 / (ramUsedGB + ramAvailGB)
		}
		if ramLoadFound {
			out.RAM.Percent = ramPct
		}
	}

	out.OK = found
	return out
}

var (
	statsMu    sync.Mutex
	statsCache statsPayload
	statsAt    time.Time
)

func collectStats(c *config) statsPayload {
	statsMu.Lock()
	defer statsMu.Unlock()

	if time.Since(statsAt) < time.Second {
		statsCache.Show = c.statsShow()
		return statsCache
	}

	p := readLHMSnapshot(c)
	statsCache = p
	statsAt = time.Now()
	return p
}

func handleStats(c *config, w http.ResponseWriter, r *http.Request) {
	if !monitoringEnabled {
		writeJSON(w, 404, map[string]string{"error": "monitoring disabled"})
		return
	}
	if !authorized(c, r) {
		logger.Printf("unauthorized stats from=%s", clientIP(r))
		writeJSON(w, 401, map[string]string{"error": "unauthorized"})
		return
	}
	writeJSON(w, 200, collectStats(c))
}
