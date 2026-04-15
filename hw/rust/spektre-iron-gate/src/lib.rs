//! SPDX-License-Identifier: AGPL-3.0-or-later
//! Parity with `rtl/cos_agency_iron_combo.sv` control triad (0 = EXECUTE, 1 = HOLD, 2 = STABILIZE).

/// trap_mask bit 6 = ANCHOR_OVR (matches SV)
pub fn agency_ctrl(
    trap_mask: u8,
    state_declared: bool,
    anchor_ok: bool,
    event_state_amp: bool,
    urgency_only: bool,
) -> u8 {
    let m = trap_mask & 0x7f;
    let pc = m.count_ones();
    let anchor_ov = (m & 0x40) != 0;
    let stab = event_state_amp || pc >= 2 || anchor_ov;
    let any_trap = m != 0;
    let naked = !state_declared || !anchor_ok;
    let hold = any_trap || naked || urgency_only;
    if stab {
        2
    } else if hold {
        1
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clean_execute() {
        assert_eq!(agency_ctrl(0, true, true, false, false), 0);
    }

    #[test]
    fn two_traps_stabilize() {
        assert_eq!(agency_ctrl(0x03, true, true, false, false), 2);
    }

    #[test]
    fn anchor_override_stabilize() {
        assert_eq!(agency_ctrl(0x40, true, true, false, false), 2);
    }

    #[test]
    fn single_trap_hold() {
        assert_eq!(agency_ctrl(0x01, true, true, false, false), 1);
    }

    #[test]
    fn urgency_only_hold() {
        assert_eq!(agency_ctrl(0, true, true, false, true), 1);
    }

    /// Structural mirror of `rtl/cos_agency_iron_combo.sv` assign chain (parity vs policy function).
    fn agency_ctrl_sv_wires(
        trap_mask: u8,
        state_declared: bool,
        anchor_ok: bool,
        event_state_amp: bool,
        urgency_only: bool,
    ) -> u8 {
        let m = trap_mask & 0x7f;
        let pc = m.count_ones();
        let anchor_ov = (m & 0x40) != 0;
        let stab = event_state_amp || pc >= 2 || anchor_ov;
        let anytrap = m != 0;
        let naked = !state_declared || !anchor_ok;
        let hold = anytrap || naked || urgency_only;
        if stab {
            2
        } else if hold {
            1
        } else {
            0
        }
    }

    #[test]
    fn exhaustive_policy_matches_sv_assign_chain() {
        for trap in 0u8..=0x7f {
            for sd in [false, true] {
                for ao in [false, true] {
                    for amp in [false, true] {
                        for urgent in [false, true] {
                            let a = agency_ctrl(trap, sd, ao, amp, urgent);
                            let b = agency_ctrl_sv_wires(trap, sd, ao, amp, urgent);
                            assert_eq!(a, b, "trap={trap} sd={sd} ao={ao} amp={amp} u={urgent}");
                        }
                    }
                }
            }
        }
    }
}
