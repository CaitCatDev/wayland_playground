use std::ffi::*;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct wl_display {
    _unused: [u8; 0],
}

type wl_event_loop = c_void;

#[link(name="wayland-server", kind="dylib")]
extern "C" {
    pub fn wl_display_create() -> *mut wl_display;
    pub fn wl_display_add_socket_auto(display: *mut wl_display) -> *mut c_char;
    pub fn wl_display_run(display: *mut wl_display);
    pub fn wl_display_terminate(display: *mut wl_display);
    pub fn wl_display_destroy(display: *mut wl_display);
    pub fn wl_display_get_event_loop(display: *mut wl_display) -> *mut wl_event_loop;
}


fn main() {
    let wl_display: *mut wl_display;
    let wl_event_loop: *mut wl_event_loop;
    let sock_path: *mut c_char;
    
    unsafe { 
        wl_display = wl_display_create();
        
        sock_path = wl_display_add_socket_auto(wl_display);

        wl_event_loop = wl_display_get_event_loop(wl_display);

        wl_display_run(wl_display);
    }
}
