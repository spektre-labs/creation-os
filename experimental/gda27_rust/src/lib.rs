//! Tier-3 GDA base-27 literal codec — Rust sidecar for `creation_os_v27` (optional link).
//! Alphabet matches `src/tokenizer/gda27_stub.c`.

const DIGITS: &[u8; 27] = b"0123456789abcdefghijklmnopq";

fn digit_index(ch: u8) -> Option<u32> {
    let c = ch.to_ascii_lowercase();
    DIGITS.iter().position(|&d| d == c).map(|i| i as u32)
}

fn encode_to_buf(mut v: u32, out: &mut [u8]) -> Option<usize> {
    if out.is_empty() {
        return None;
    }
    if v == 0 {
        out[0] = DIGITS[0];
        return Some(1);
    }
    let mut tmp = [0u8; 16];
    let mut n = 0usize;
    let mut x = v;
    while x > 0 && n < tmp.len() {
        let d = (x % 27) as usize;
        tmp[n] = DIGITS[d];
        n += 1;
        x /= 27;
    }
    if n > out.len() {
        return None;
    }
    let mut w = 0usize;
    let mut i = n;
    while i > 0 {
        i -= 1;
        out[w] = tmp[i];
        w += 1;
    }
    Some(w)
}

#[no_mangle]
pub unsafe extern "C" fn creation_os_gda27_rust_encode(v: u32, out: *mut u8, cap: i32) -> i32 {
    if out.is_null() || cap < 2 {
        return -1;
    }
    let buf = std::slice::from_raw_parts_mut(out, cap as usize);
    match encode_to_buf(v, &mut buf[..(cap as usize).saturating_sub(1)]) {
        Some(n) => {
            buf[n] = 0;
            n as i32
        }
        None => -1,
    }
}

#[no_mangle]
pub unsafe extern "C" fn creation_os_gda27_rust_decode(s: *const u8) -> u32 {
    if s.is_null() {
        return 0;
    }
    let mut acc: u32 = 0;
    let mut p = s;
    loop {
        let ch = *p;
        if ch == 0 {
            break;
        }
        if let Some(idx) = digit_index(ch) {
            acc = acc.saturating_mul(27).saturating_add(idx);
        } else {
            break;
        }
        p = p.add(1);
    }
    acc
}
