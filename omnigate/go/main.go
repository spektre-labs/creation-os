// Omnigate Go runner — stdlib only, zero deps.
package main

import (
	"os"
	"os/exec"
	"syscall"
)

func main() {
	root := os.Getenv("CREATION_OS_ROOT")
	if root == "" {
		root = "."
	}
	cmd := exec.Command("make", "merge-gate")
	cmd.Dir = root
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin
	if err := cmd.Run(); err != nil {
		if exitErr, ok := err.(*exec.ExitError); ok {
			if st, ok := exitErr.Sys().(syscall.WaitStatus); ok {
				os.Exit(st.ExitStatus())
			}
		}
		os.Exit(1)
	}
}
