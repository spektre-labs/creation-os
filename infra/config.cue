// Cue — schema for infra entrypoints (cosmetic contract; vet with `cue vet infra/config.cue`).
package cos

#Toolchain: {
	name:     string
	role:     string
	optional: bool | *false
}

toolchains: [...#Toolchain] & [
	{name: "clang+c11", role: "portable kernel", optional: false},
	{name: "yosys", role: "rtl elaborate / comb", optional: true},
	{name: "symbiyosys", role: "sby formal", optional: true},
	{name: "verilator", role: "lint-only rtl", optional: true},
	{name: "rustc+cargo", role: "spektre-iron-gate", optional: true},
	{name: "jdk17+sbt", role: "chisel", optional: true},
]

merge_gate: {
	command: "make"
	target:  "merge-gate"
	note:    "portable check + check-v6 .. check-v26"
}
