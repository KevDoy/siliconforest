//! Experimental native Darwin frontend. No Linux device backend is selected.
#![cfg(feature = "darwin-backend")]

pub use smithay;
pub mod applications;
#[cfg(feature = "desktop-shell")]
pub mod desktop;
pub mod hid;
mod input;
pub mod launch_audit;
pub mod pixels;
pub mod presenter;
pub mod scene;
pub mod server;
pub mod terminal_launch;
