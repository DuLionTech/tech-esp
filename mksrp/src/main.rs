use srp6::prelude::*;
use std::fs;
use srp6::rfc_5054_appendix_a::group_3072_bit::{g, N};
use uuid::Uuid;

type Srp6_esp32 = Srp6<384, 16>;

fn main() {
    // this is what a user would enter in a form / terminal
    let new_username: UsernameRef = "alice";
    let user_password: ClearTextPasswordRef = "password123";

    // Reminder: choose always a Srp6_BITS type that is strong like 2048 or 4096
    let srp = Srp6_esp32::new(g(), N()).unwrap();
    
    let (salt_s, verifier_v) = srp.generate_new_user_secrets(new_username, user_password);

    fs::write("salt.bin", salt_s.to_vec()).unwrap();
    fs::write("verifier.bin", verifier_v.to_vec()).unwrap();
    fs::write("uuid.bin", Uuid::new_v4().as_bytes().to_vec()).unwrap();
}