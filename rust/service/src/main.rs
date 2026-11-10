macro_rules! eprintln {
    ($($arguments:tt)*) => { crate::logging::error(format_args!($($arguments)*)) };
}

// Project routine diagnostics are compile-time opt-in.
macro_rules! facehal_trace {
    ($($arguments:tt)*) => {{
        if matches!(option_env!("FACEHAL_VERBOSE_LOGS"), Some("1")) {
            crate::logging::trace(format_args!($($arguments)*));
        }
    }};
}

mod logging;
mod algorithm;
mod authenticator;
mod camera;
mod cancellation;
mod face;
mod preview;
mod session;

use android_hardware_biometrics_face::aidl::android::hardware::biometrics::face::IFace::BnFace;
use binder::{add_service, BinderFeatures, ProcessState};
use face::FaceService;

const INSTANCE: &str = "android.hardware.biometrics.face.IFace/default";

fn main() {
    if let Some(exit_code) = camera::run_worker_from_args() {
        std::process::exit(exit_code);
    }

    ProcessState::set_thread_pool_max_thread_count(4);
    let service = BnFace::new_binder(FaceService::new(), BinderFeatures::default());
    if let Err(error) = add_service(INSTANCE, service.as_binder()) {
        eprintln!("FaceHAL registration failed: {error:?}; see the raw exception above");
        std::process::exit(1);
    }
    facehal_trace!("FaceHAL registered {INSTANCE}");
    ProcessState::join_thread_pool();
}
