
const y = 5;

fun what(a: int, b: int) -> int {
    if a > b {
        return a + b;
    }
    else {
        return a - b;
    }
}

const x = what(2, 5) + 6;
