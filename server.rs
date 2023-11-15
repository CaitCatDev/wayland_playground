
use std::ffi::*;


#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct {
    
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct wl_display {
    _unused: [u8; 0],
} 

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct wl_registry {
    _unused: [u8; 0],
} 

#[link(name="wayland-server", kind="dylib")]
extern "C" {
    pub fn wl_display_create() -> *mut wl_display;
    pub fn wl_display_add_socket_auto(display: *mut wl_display) -> *mut c_char;
    pub fn wl_display_run(display: *mut wl_display);
}


fn main() {
    let wl_display: *mut wl_display;
    let sock_path: *mut c_char;
    unsafe { 
        wl_display = wl_display_create();

        sock_path = wl_display_add_socket_auto(wl_display);
 
    }
    
    println!("WL_Display Running On Socket: {}", unsafe {CStr::from_ptr(sock_path as *const _)}.to_string_lossy() );

    unsafe {
        wl_display_run(wl_display);
    }
}
