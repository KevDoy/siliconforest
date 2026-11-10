//! Real shared Shell over the CPU backend. No alternate menu/window policy.
//! Native launch remains unavailable until root binds the shim and application.
use crate::{
    applications::{Instance, LaunchDisposition, OwnedApplications},
    scene::CpuBackend,
    terminal_launch::TerminalLauncher,
};
use chonk_shell::{
    managed::LaunchResult,
    shell::{Shell, ShellOutcome},
};
use wm_core::KeyCombo;
use wm_core::{MouseButton, Point, WindowManager};

pub struct DesktopSession {
    pub shell: Shell<CpuBackend>,
    pub applications: OwnedApplications,
    pub closing: bool,
    launcher: Option<TerminalLauncher>,
}

impl crate::server::Server {
    /// Explicit library entry; the existing native CLI does not select it.
    /// Source/host testing only until frontend/native dependencies are admitted.
    pub fn enable_desktop_shell(&mut self, fonts: wm_theme::FontState) -> Result<(), String> {
        if self.desktop.is_some() {
            return Err("desktop Shell already enabled".into());
        }
        if !self.desktop_seat_ready() {
            return Err("standalone desktop requires the actual Both seat".into());
        }
        self.desktop = Some(DesktopSession::new(&mut self.wm, fonts)?);
        Ok(())
    }
    /// Root's future admitted entry must supply bound artifacts/private paths.
    /// There is no default path, env activation, native CLI, or fallback app.
    pub fn configure_managed_applications(
        &mut self,
        binary: &std::path::Path,
        socket: &std::path::Path,
        records: &std::path::Path,
        session: Instance,
        lifetime: std::time::Duration,
    ) -> Result<(), String> {
        let desktop = self.desktop.as_mut().ok_or("desktop Shell is absent")?;
        if desktop.launcher.is_some() || desktop.closing {
            return Err("managed applications already configured or closing".into());
        }
        desktop.launcher = Some(
            TerminalLauncher::new(
                binary,
                socket,
                records,
                session,
                lifetime,
                &mut desktop.applications,
            )
            .map_err(|e| e.to_string())?,
        );
        Ok(())
    }
    pub(crate) fn service_desktop(&mut self) -> Result<(), String> {
        let Some(mut desktop) = self.desktop.take() else {
            return Ok(());
        };
        let result = desktop.service(&mut self.wm);
        self.desktop = Some(desktop);
        result
    }
    pub(crate) fn desktop_key(&mut self, combo: KeyCombo, down: bool) -> Result<bool, String> {
        use chonk_shell::shell::KeyResolution;
        let Some(mut desktop) = self.desktop.take() else {
            return Ok(false);
        };
        let resolution = if down {
            desktop.shell.keymap_action(&combo)
        } else {
            desktop
                .shell
                .keymap_release_action(&combo)
                .map(KeyResolution::Action)
        };
        let consumed = resolution.is_some();
        let outcome = match resolution {
            Some(KeyResolution::Action(action)) => desktop.shell.run_action(&mut self.wm, &action),
            Some(KeyResolution::Menu(key)) => desktop.shell.run_menu_key(&mut self.wm, key),
            Some(KeyResolution::Consumed) | None => ShellOutcome::Continue,
        };
        let result = desktop.outcome(outcome).map(|_| consumed);
        self.desktop = Some(desktop);
        result
    }
    /// Returns true only when real shell/root ownership consumes this event.
    /// Existing client/WM implicit drags must bypass this before calling.
    pub(crate) fn desktop_pointer(
        &mut self,
        root: Point,
        buttons: u8,
        previous: u8,
    ) -> Result<bool, String> {
        use crate::scene::DesktopHit;
        if self.desktop.is_none() {
            return Ok(false);
        }
        let hit = self.wm.backend().desktop_hit(root);
        let was_popup = self.wm.backend().popup_active();
        if self.wm.backend().keyboard_grabbed && !was_popup {
            return Ok(true);
        }
        if !was_popup && matches!(hit, DesktopHit::Frame(..) | DesktopHit::Client(..)) {
            return Ok(false);
        }
        let mut desktop = self.desktop.take().unwrap();
        if let DesktopHit::Shell(id, local) = hit {
            self.wm.backend_mut().queue_shell_motion(id, local);
        }
        desktop.shell.on_motion(&mut self.wm, root);
        let mut result = Ok(());
        for bit in 0..3 {
            let mask = 1 << bit;
            if (previous ^ buttons) & mask == 0 {
                continue;
            }
            let pressed = buttons & mask != 0;
            let button = match bit {
                0 => MouseButton::Left,
                1 => MouseButton::Right,
                _ => MouseButton::Middle,
            };
            let outcome = match hit {
                DesktopHit::Shell(id, local) => {
                    desktop
                        .shell
                        .on_shell_click(&mut self.wm, id, local, button, pressed)
                }
                _ if was_popup => {
                    desktop.shell.cancel_transient_menus(&mut self.wm);
                    ShellOutcome::Continue
                }
                DesktopHit::Root if pressed => {
                    desktop.shell.on_root_press(&mut self.wm, root, button)
                }
                _ => ShellOutcome::Continue,
            };
            if let Err(error) = desktop.outcome(outcome) {
                result = Err(error);
                break;
            }
        }
        self.desktop = Some(desktop);
        result.map(|_| true)
    }
}
impl DesktopSession {
    pub fn new(
        wm: &mut WindowManager<CpuBackend>,
        fonts: wm_theme::FontState,
    ) -> Result<Self, String> {
        if !wm.backend().input_enabled {
            return Err("desktop requires an acquired input seat".into());
        }
        let mut shell = Shell::new_standalone_darwin(wm.backend_mut(), fonts.clone());
        wm.set_theme_engine(Box::new(shell.session_state().decoration_engine(fonts)));
        wm.bind_default_keys();
        shell.initialize_window_manager(wm);
        if !wm.backend().keyboard_config_verified {
            return Err("standalone keyboard contract not installed".into());
        }
        shell.apply_workareas(wm);
        Ok(Self {
            shell,
            applications: OwnedApplications::default(),
            closing: false,
            launcher: None,
        })
    }
    pub fn outcome(&mut self, outcome: ShellOutcome) -> Result<(), String> {
        match outcome {
            ShellOutcome::Continue => Ok(()),
            ShellOutcome::Exit => {
                self.closing = true;
                self.applications.begin_shutdown();
                Ok(())
            }
            ShellOutcome::Restart | ShellOutcome::Capture(_) => {
                Err("standalone policy leaked unsupported outcome".into())
            }
        }
    }
    pub fn service(&mut self, wm: &mut WindowManager<CpuBackend>) -> Result<(), String> {
        if self
            .launcher
            .as_ref()
            .is_some_and(TerminalLauncher::deadline_reached)
        {
            self.closing = true;
            self.applications.begin_shutdown();
        }
        while let Some(n) = wm.take_notification() {
            self.shell.on_notification(wm, n);
        }
        self.shell.tick(wm);
        while let Some(request) = self.shell.managed().and_then(|b| b.take_request()) {
            let result = if let Some(launcher) = self.launcher.as_mut() {
                match launcher.request(request.id, &mut self.applications) {
                    Ok(LaunchDisposition::Created {
                        launch_failed: false,
                        ..
                    }) => LaunchResult::ProcessCreated,
                    Ok(LaunchDisposition::Created {
                        launch_failed: true,
                        ..
                    }) => LaunchResult::Refused(
                        "process created with native cleanup failure; owned shutdown pending"
                            .into(),
                    ),
                    Err(error) => LaunchResult::Refused(error),
                }
            } else {
                self.applications.refuse_unadmitted_request(request.id)?;
                LaunchResult::Refused("native spawn/app not admitted".into())
            };
            let failed = matches!(result, LaunchResult::Refused(_));
            self.shell.complete_managed_request(request.id, result)?;
            if failed && wm.backend().popup_can_open() {
                // Show the actual refusal in the same real root cascade.
                let at = wm.backend().pointer;
                self.shell.on_root_press(wm, at, MouseButton::Right);
            }
        }
        self.applications.poll();
        if self.applications.shutdown_incomplete() || self.applications.audit.error().is_some() {
            self.closing = true;
            self.applications.begin_shutdown();
        }
        if let Some(error) = wm.backend().input_fault.as_ref() {
            return Err(error.clone());
        }
        if self.closing {
            self.shell.begin_managed_shutdown(wm);
        }
        Ok(())
    }
    pub fn root_press(
        &mut self,
        wm: &mut WindowManager<CpuBackend>,
        at: Point,
        button: MouseButton,
    ) -> Result<(), String> {
        if !wm.backend().popup_can_open() && !wm.backend().popup_active() {
            return Err("root gesture conflicts with an owned grab".into());
        }
        let outcome = self.shell.on_root_press(wm, at, button);
        self.outcome(outcome)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::scene::DesktopHit;
    use wm_core::{Rect, Size};
    use wm_theme_api::PopupHost;
    fn backend() -> CpuBackend {
        let mut b = CpuBackend::new(Size::new(640, 480)).unwrap();
        b.input_enabled = true;
        b
    }
    #[test]
    fn actual_popup_storage_hit_order_and_release() {
        let mut b = backend();
        let p = b
            .create_popup(
                Rect::new(Point::new(20, 30), Size::new(40, 50)),
                (10, 20, 30),
            )
            .unwrap();
        assert_eq!(
            b.desktop_hit(Point::new(21, 31)),
            DesktopHit::Shell(p, Point::new(1, 1))
        );
        assert_eq!(b.desktop_hit(Point::new(60, 80)), DesktopHit::Root);
        let q = b
            .create_popup(
                Rect::new(Point::new(20, 30), Size::new(40, 50)),
                (40, 50, 60),
            )
            .unwrap();
        assert_eq!(
            b.desktop_hit(Point::new(21, 31)),
            DesktopHit::Shell(q, Point::new(1, 1))
        );
        let token = PopupHost::grab_pointer(&mut b);
        PopupHost::grab_keyboard(&mut b);
        b.destroy_popup(q);
        b.destroy_popup(p);
        assert!(b.popup_active());
        PopupHost::ungrab_keyboard(&mut b);
        PopupHost::ungrab_pointer(&mut b, token);
        assert_eq!(b.desktop_hit(Point::new(21, 31)), DesktopHit::Root);
        assert!(b.popup_can_open());
    }
    #[test]
    fn actual_shared_shell_constructor_menu_and_launch_refusal() {
        let fonts = wm_theme::FontState::new();
        let mut wm = WindowManager::new(
            backend(),
            Box::new(wm_theme::RasterThemeEngine::with_fonts(
                wm_theme::default_theme::nextstep_classic(),
                fonts.clone(),
            )),
        );
        let mut session = DesktopSession::new(&mut wm, fonts).unwrap();
        assert!(session.shell.is_standalone());
        assert!(session.shell.extra_poll_fds().is_empty());
        assert!(!session.shell.session_state().reads_hyprland_config);
        session
            .root_press(&mut wm, Point::new(20, 20), MouseButton::Right)
            .unwrap();
        assert!(wm.backend().popup_active());
        assert!(wm.backend().keyboard_grabbed);
        // Real shared menu navigation resolves its real first Terminal row.
        let outcome = session
            .shell
            .run_menu_key(&mut wm, wm_theme::cascade::MenuKey::Enter);
        session.outcome(outcome).unwrap();
        session.service(&mut wm).unwrap();
        assert_eq!(
            session.shell.managed().unwrap().last_failure(),
            Some("native spawn/app not admitted")
        );
        assert!(session.applications.applications().is_empty());
        assert_eq!(wm.backend().render().unwrap().width(), 640);
        session.shell.begin_managed_shutdown(&mut wm);
        assert!(!wm.backend().popup_active());
        assert!(!wm.backend().keyboard_grabbed);
    }
}
