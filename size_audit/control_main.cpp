// Baseline skeleton -- deliberately never includes any lweh header. Its
// measured size is the "platform floor" that size_report.py subtracts out
// of the with-lweh build (research.md §B6, §B9). Plain main(): this target
// links with the default C runtime start (no -nostdlib/custom linker
// script, unlike examples/*_minimal/), so it needs the entry point its
// actual link configuration expects, on host or embedded alike.

int main() {
    return 0;
}
