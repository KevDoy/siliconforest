//! Bounded request bridge for the real Shell's standalone platform profile.
//! It never spawns, performs IPC, or labels a queued request as a running app.
use std::collections::{BTreeSet, VecDeque};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TerminalRequest {
    pub id: u64,
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum LaunchResult {
    ProcessCreated,
    Refused(String),
}
#[derive(Default)]
pub struct ManagedBridge {
    next: u64,
    pending: VecDeque<TerminalRequest>,
    outstanding: BTreeSet<u64>,
    closing: bool,
    last_failure: Option<String>,
}
impl ManagedBridge {
    pub fn request_terminal(&mut self) -> Result<u64, &'static str> {
        if self.closing {
            return Err("session is closing");
        }
        if self.outstanding.len() >= 4 {
            return Err("managed launch queue full");
        }
        let next = self
            .next
            .checked_add(1)
            .ok_or("managed launch identity exhausted")?;
        self.next = next;
        self.pending.push_back(TerminalRequest { id: next });
        self.outstanding.insert(next);
        Ok(next)
    }
    pub fn take_request(&mut self) -> Option<TerminalRequest> {
        self.pending.pop_front()
    }
    pub fn complete(&mut self, request: u64, result: LaunchResult) -> Result<(), &'static str> {
        if self.pending.iter().any(|r| r.id == request) || !self.outstanding.remove(&request) {
            return Err("request was not handed to the platform or was already completed");
        }
        if let LaunchResult::Refused(reason) = result {
            self.last_failure = Some(reason.chars().take(160).collect());
        }
        Ok(())
    }
    pub fn refuse(&mut self, reason: &str) {
        self.last_failure = Some(reason.chars().take(160).collect());
    }
    pub fn last_failure(&self) -> Option<&str> {
        self.last_failure.as_deref()
    }
    pub fn begin_shutdown(&mut self) {
        self.closing = true;
        // Requests already handed out remain outstanding until the platform
        // records their outcome; queued ones are explicitly canceled here.
        for request in self.pending.drain(..) {
            self.outstanding.remove(&request.id);
        }
    }
    pub fn closing(&self) -> bool {
        self.closing
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn request_is_not_success_and_completes_once() {
        let mut bridge = ManagedBridge::default();
        let id = bridge.request_terminal().unwrap();
        assert!(bridge.complete(id, LaunchResult::ProcessCreated).is_err());
        assert_eq!(bridge.take_request(), Some(TerminalRequest { id }));
        bridge
            .complete(id, LaunchResult::Refused("native shim unavailable".into()))
            .unwrap();
        assert_eq!(bridge.last_failure(), Some("native shim unavailable"));
        assert!(bridge.complete(id, LaunchResult::ProcessCreated).is_err());
    }
    #[test]
    fn outstanding_bound_and_shutdown_preserve_handed_request() {
        let mut bridge = ManagedBridge::default();
        for _ in 0..4 {
            bridge.request_terminal().unwrap();
        }
        assert!(bridge.request_terminal().is_err());
        let active = bridge.take_request().unwrap();
        bridge.begin_shutdown();
        assert!(bridge.take_request().is_none());
        assert!(bridge.request_terminal().is_err());
        bridge
            .complete(active.id, LaunchResult::Refused("closing".into()))
            .unwrap();
        assert!(bridge.closing());
    }
}
