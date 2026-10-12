//! WebCLAP host scratch allocation, separate from the processing implementation.
//! Pointer ownership follows malloc/free: only release pointers returned here.
use std::alloc::{Layout, alloc, dealloc};

#[unsafe(no_mangle)]
/// Allocate a 16-byte-aligned host buffer, or return null on failure.
///
/// # Safety
/// The host must not access more than `size` bytes or use the buffer after free.
pub unsafe extern "C" fn malloc(size: usize) -> *mut u8 {
    let Some(total) = size.checked_add(16) else {
        return std::ptr::null_mut();
    };
    let Ok(layout) = Layout::from_size_align(total, 16) else {
        return std::ptr::null_mut();
    };
    // SAFETY: Valid checked layout; the header records the same layout for free.
    unsafe {
        let base = alloc(layout);
        if base.is_null() {
            return base;
        }
        base.cast::<usize>().write(total);
        base.add(16)
    }
}

#[unsafe(no_mangle)]
/// Release a live allocation returned by this module's `malloc`.
///
/// # Safety
/// `pointer` must be null or an unmodified, not-yet-freed result of `malloc`.
pub unsafe extern "C" fn free(pointer: *mut u8) {
    if pointer.is_null() {
        return;
    }
    // SAFETY: The host must supply a live pointer obtained from malloc above.
    unsafe {
        let base = pointer.sub(16);
        let total = base.cast::<usize>().read();
        dealloc(base, Layout::from_size_align_unchecked(total, 16));
    }
}
