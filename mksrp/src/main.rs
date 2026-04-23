use srp6::prelude::*;
use std::fs::File;
use std::io::Write;
use srp6::rfc_5054_appendix_a::group_3072_bit::{g, N};
use uuid::Uuid;

type Srp6esp32 = Srp6<384, 16>;

fn main() {
    // this is what a user would enter in a form / terminal
    let new_username: UsernameRef = "alice";
    let user_password: ClearTextPasswordRef = "password123";

    // Reminder: choose always a Srp6_BITS type that is strong like 2048 or 4096
    let srp = Srp6esp32::new(g(), N()).unwrap();

    let (salt_s, verifier_v) = srp.generate_new_user_secrets(new_username, user_password);

    let mut data = File::create("data.bin").unwrap();
    data.write(&Uuid::new_v4().as_bytes().to_vec()).expect("Failed to write service id");
    data.write(&salt_s.to_vec()).expect("Failed to write salt");
    data.write(&verifier_v.to_vec()).expect("Failed to write verifier");
    data.flush().expect("Failed to flush data");
}