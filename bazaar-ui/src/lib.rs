mod scrolled_window;

pub use scrolled_window::*;

#[no_mangle]
pub extern "C" fn bazaar_ui_init() {
    _ = gtk::init();
}
