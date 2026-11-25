"""MapLibre Native's checks for `tools/ready.py`: format and workflow lint, then the core's tests."""

from ready import Check

CORE = ("src/*", "include/*", "test/*", "CMakeLists.txt", "cmake/*")
IOS = ("platform/ios/*", "platform/darwin/*", "src/*", "include/*", "BUILD.bazel", "MODULE.bazel")
CORE_TESTS = (
    "cmake --preset linux-opengl > /dev/null && cmake --build build-linux-opengl --target mbgl-test-runner"
    " && xvfb-run -a build-linux-opengl/mbgl-test-runner"
)

# a fork of upstream: a file is held to its format once the branch touches it
CHECKS = [
    Check("ready's tests", ["python3", "tools/test-ready.py"], covers=("tools/*ready*",)),
    Check(
        "format, C++",
        ["clang-format", "--dry-run", "--Werror"],
        covers=("src/*.[ch]pp", "include/*.[ch]pp", "test/*.[ch]pp", "platform/*.[ch]pp", "*.h", "*.mm"),
        tool="clang-format==20.1.8",
        on_paths=True,
    ),
    Check(
        "format, Swift",
        ["swiftformat", "--lint", "--swiftversion", "5.8"],
        covers=("*.swift",),
        needs="swiftformat",
        on_paths=True,
    ),
    Check(
        "workflows",
        ["actionlint"],
        covers=(".github/workflows/*.yml", ".github/workflows/*.yaml"),
        tool="actionlint-py==1.7.12.24",
        on_paths=True,
    ),
    Check("unit tests, core", ["sh", "-c", CORE_TESTS], covers=CORE, needs="xvfb-run", waits=True, limit=120 * 60),
    Check(
        "unit tests, Riki",
        ["bazel", "test", "--//:renderer=metal", "//platform/ios/riki-test:riki_test"],
        covers=IOS,
        needs="darwin",
        waits=True,
        limit=120 * 60,
    ),
]
