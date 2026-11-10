use std::fmt;
#[cfg(target_os = "android")]
use std::ffi::CString;

#[cfg(target_os = "android")]
unsafe extern "C" {
    fn __android_log_write(priority: i32, tag: *const std::ffi::c_char, text: *const std::ffi::c_char) -> i32;
}

fn write(priority: i32, arguments: fmt::Arguments<'_>) {
    #[cfg(target_os = "android")]
    {
        let tag = CString::new(option_env!("FACEHAL_LOG_TAG").unwrap_or("FaceHAL")).expect("valid log tag");
        let text = CString::new(arguments.to_string().replace('\0', " ")).expect("sanitized log text");
        unsafe { __android_log_write(priority, tag.as_ptr(), text.as_ptr()); }
    }
    #[cfg(not(target_os = "android"))]
    {
        let _ = priority;
        std::eprintln!("{arguments}");
    }
}

pub fn error(arguments: fmt::Arguments<'_>) { write(6, arguments); }
pub fn trace(arguments: fmt::Arguments<'_>) { write(4, arguments); }
