package main

// Shared monitoring config types. Kept outside the !lite build so the lite
// binary can still parse the same config.json (and simply ignore monitoring).

type statsShow struct {
	CPULoad bool `json:"cpu_load"`
	CPUTemp bool `json:"cpu_temp"`
	GPULoad bool `json:"gpu_load"`
	GPUTemp bool `json:"gpu_temp"`
	VRAM    bool `json:"vram"`
	RAM     bool `json:"ram"`
}

type statsConfig struct {
	Show statsShow `json:"show"`
}

type lhmConfig struct {
	AutoStart *bool  `json:"auto_start"`
	Path      string `json:"path"`
	HTTPPort  int    `json:"http_port"`
}

func defaultStatsShow() statsShow {
	return statsShow{
		CPULoad: true,
		CPUTemp: true,
		GPULoad: true,
		GPUTemp: true,
		VRAM:    true,
		RAM:     true,
	}
}

func (c *config) statsShow() statsShow {
	if c.Stats == nil {
		return defaultStatsShow()
	}
	return c.Stats.Show
}

// monitoringEnabled is switched off by --no-monitor: no LHM auto-start and
// /stats returns 404. The rest of the server works as usual.
var monitoringEnabled = true
