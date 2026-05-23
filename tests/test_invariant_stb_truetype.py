import pytest
import ctypes
import struct
import sys


# Simulate the integer overflow vulnerability in compound glyph vertex allocation
# The security invariant: allocation size must NEVER be smaller than the sum of
# the two vertex counts multiplied by the vertex size (no integer overflow allowed)

STBTT_VERTEX_SIZE = 20  # typical size of stbtt_vertex struct (x, y, cx, cy, type, padding)
MAX_SAFE_INT32 = 0x7FFFFFFF
MAX_UINT32 = 0xFFFFFFFF
MAX_UINT16 = 0xFFFF


def safe_allocation_size(num_vertices, comp_num_verts, vertex_size=STBTT_VERTEX_SIZE):
    """
    Compute the required allocation size safely using Python's arbitrary precision integers.
    Returns the true mathematical result without overflow.
    """
    return (num_vertices + comp_num_verts) * vertex_size


def c_uint32_overflow_size(num_vertices, comp_num_verts, vertex_size=STBTT_VERTEX_SIZE):
    """
    Simulate what a 32-bit C implementation would compute with potential overflow.
    This mimics the vulnerable C code behavior.
    """
    # Simulate 32-bit unsigned integer overflow
    sum_vertices = (num_vertices + comp_num_verts) & MAX_UINT32
    allocation = (sum_vertices * vertex_size) & MAX_UINT32
    return allocation


def c_int32_overflow_size(num_vertices, comp_num_verts, vertex_size=STBTT_VERTEX_SIZE):
    """
    Simulate what a signed 32-bit C implementation would compute with potential overflow.
    """
    sum_vertices = ctypes.c_int32(num_vertices + comp_num_verts).value
    allocation = ctypes.c_int32(sum_vertices * vertex_size).value
    return allocation


def validate_allocation_no_overflow(num_vertices, comp_num_verts, vertex_size=STBTT_VERTEX_SIZE):
    """
    Security invariant check: the computed allocation size must be sufficient
    to hold all vertices without overflow.
    
    Returns True if allocation is safe, False if overflow would occur.
    """
    true_size = safe_allocation_size(num_vertices, comp_num_verts, vertex_size)
    
    # Check if values fit in 32-bit integers (as used in C)
    if num_vertices > MAX_UINT32 or comp_num_verts > MAX_UINT32:
        return False  # Values too large for 32-bit
    
    # Check addition overflow in 32-bit
    sum_32 = (num_vertices + comp_num_verts) & MAX_UINT32
    if sum_32 < num_vertices or sum_32 < comp_num_verts:
        return False  # Addition overflowed
    
    # Check multiplication overflow in 32-bit
    alloc_32 = (sum_32 * vertex_size) & MAX_UINT32
    if alloc_32 < true_size:
        return False  # Multiplication overflowed or result truncated
    
    return True


@pytest.mark.parametrize("payload", [
    # (num_vertices, comp_num_verts, description)
    # Normal cases - should be safe
    (0, 0, "zero vertices"),
    (1, 1, "minimal vertices"),
    (100, 100, "small normal case"),
    
    # Boundary values that could trigger overflow in 32-bit arithmetic
    (MAX_UINT32, 1, "max_uint32 + 1 addition overflow"),
    (MAX_UINT32, MAX_UINT32, "max_uint32 + max_uint32 addition overflow"),
    (MAX_INT32 := 0x7FFFFFFF, 1, "max_int32 + 1 signed overflow"),
    (MAX_INT32 := 0x7FFFFFFF, MAX_INT32 := 0x7FFFFFFF, "max_int32 + max_int32 signed overflow"),
    
    # Values that cause multiplication overflow after addition
    (0x10000000, 0x10000000, "large values multiplication overflow"),
    (0x20000000, 0x20000000, "larger values multiplication overflow"),
    (0x40000000, 0x40000000, "near-max values multiplication overflow"),
    
    # Values specifically crafted to produce small allocation after overflow
    # (num + comp) * 20 wraps around to a small number
    (0xCCCCCCCD, 0x33333333, "crafted overflow to small allocation"),
    (0x0CCCCCCD, 0x03333333, "crafted overflow variant"),
    
    # Edge cases with vertex size considerations
    (0x19999999, 0x06666667, "multiplication overflow with vertex_size=20"),
    (MAX_UINT16, MAX_UINT16, "max uint16 values"),
    (MAX_UINT16 + 1, MAX_UINT16 + 1, "just over uint16 max"),
    
    # Negative-like values when interpreted as signed
    (0x80000000, 0x80000000, "sign bit set in both"),
    (0x80000001, 0x7FFFFFFF, "mixed sign boundary"),
    
    # Values from font file that could be attacker-controlled
    (65535, 65535, "max glyph count values"),
    (32768, 32768, "half max glyph count"),
    (0xFFFF, 0x0001, "max uint16 plus one"),
    
    # Zero and one edge cases
    (0, MAX_UINT32, "zero plus max"),
    (1, MAX_UINT32, "one plus max"),
    (MAX_UINT32, 0, "max plus zero"),
])
def test_vertex_allocation_no_integer_overflow(payload):
    """
    Invariant: The allocation size for compound glyph vertices must NEVER be smaller
    than the true mathematical size required to hold all vertices. Any integer overflow
    in the addition (num_vertices + comp_num_verts) or subsequent multiplication by
    vertex_size must be detected and rejected before memory allocation occurs.
    
    If overflow is detected, the allocation must either fail safely or use the
    correct (non-overflowed) size. Under no circumstances should a buffer be
    allocated that is smaller than the data written into it.
    """
    num_vertices, comp_num_verts, description = payload
    vertex_size = STBTT_VERTEX_SIZE
    
    # Compute the true required size using Python's arbitrary precision
    true_required_size = safe_allocation_size(num_vertices, comp_num_verts, vertex_size)
    
    # Compute what a naive 32-bit C implementation would allocate
    c32_allocated = c_uint32_overflow_size(num_vertices, comp_num_verts, vertex_size)
    
    # SECURITY INVARIANT: If the C implementation would allocate less than required,
    # this MUST be detected as an overflow condition and handled safely.
    # The test verifies that we can DETECT when overflow would occur.
    
    overflow_detected = not validate_allocation_no_overflow(num_vertices, comp_num_verts, vertex_size)
    
    if overflow_detected:
        # When overflow is detected, the C allocation would be insufficient
        # The invariant: overflow detection must be accurate
        # If we say overflow occurred, the C size must indeed be wrong
        assert c32_allocated < true_required_size or true_required_size > MAX_UINT32, (
            f"[{description}] Overflow detected but C allocation ({c32_allocated}) "
            f">= true required ({true_required_size}). Detection may be incorrect."
        )
    else:
        # When no overflow is detected, the C allocation must be sufficient
        # The invariant: safe cases must actually be safe
        assert c32_allocated >= true_required_size, (
            f"[{description}] No overflow detected but C allocation ({c32_allocated}) "
            f"< true required ({true_required_size}). This is a FALSE NEGATIVE - "
            f"a heap buffer overflow would occur!"
        )
        
        # Additional check: the allocation must be non-negative
        assert c32_allocated >= 0, (
            f"[{description}] Allocation size is negative: {c32_allocated}"
        )
        
        # The sum must not overflow 32-bit
        sum_check = num_vertices + comp_num_verts
        assert sum_check <= MAX_UINT32, (
            f"[{description}] Sum {sum_check} exceeds 32-bit max but no overflow detected"
        )


@pytest.mark.parametrize("payload", [
    (0x0CCCCCCE, 0x33333332, "specific overflow to near-zero allocation"),
    (214748365, 214748364, "values near int32 max / vertex_size"),
    (107374183, 107374182, "values that overflow after multiplication"),
])
def test_multiplication_overflow_detection(payload):
    """
    Invariant: Multiplication overflow in (num_vertices + comp_num_verts) * sizeof(vertex)
    must be detectable. The product must fit within the target integer type, or the
    operation must be rejected.
    """
    num_vertices, comp_num_verts, description = payload
    vertex_size = STBTT_VERTEX_SIZE
    
    true_sum = num_vertices + comp_num_verts
    true_product = true_sum * vertex_size
    
    # Simulate 32-bit multiplication
    c32_product = (true_sum * vertex_size) & MAX_UINT32
    
    # INVARIANT: If the true product exceeds 32-bit max, the C code would
    # produce an undersized allocation - this MUST be caught
    if true_product > MAX_UINT32:
        # The C code would produce wrong result
        assert c32_product < true_product, (
            f"[{description}] Expected overflow: true={true_product}, c32={c32_product}"
        )
        # Verify our detection catches this
        assert not validate_allocation_no_overflow(num_vertices, comp_num_verts, vertex_size), (
            f"[{description}] Overflow not detected! true_product={true_product} > MAX_UINT32"
        )
    else:
        # No overflow - C result should match true result
        assert c32_product == true_product, (
            f"[{description}] Unexpected mismatch: true={true_product}, c32={c32_product}"
        )


def test_safe_small_values_always_pass():
    """
    Invariant: Small, legitimate vertex counts must always produce correct allocations.
    This ensures the security check doesn't break normal functionality.
    """
    safe_cases = [
        (0, 0),
        (1, 0),
        (0, 1),
        (1, 1),
        (10, 10),
        (100, 50),
        (1000, 1000),
        (10000, 10000),
    ]
    
    for num_v, comp_v in safe_cases:
        true_size = safe_allocation_size(num_v, comp_v)
        c32_size = c_uint32_overflow_size(num_v, comp_v)
        is_safe = validate_allocation_no_overflow(num_v, comp_v)
        
        assert is_safe, f"Safe case ({num_v}, {comp_v}) incorrectly flagged as overflow"
        assert c32_size == true_size, (
            f"Safe case ({num_v}, {comp_v}): c32={c32_size} != true={true_size}"
        )
        assert c32_size >= num_v * STBTT_VERTEX_SIZE, (
            f"Allocation too small for first buffer: {c32_size} < {num_v * STBTT_VERTEX_SIZE}"
        )
        assert c32_size >= comp_v * STBTT_VERTEX_SIZE, (
            f"Allocation too small for second buffer: {c32_size} < {comp_v * STBTT_VERTEX_SIZE}"
        )