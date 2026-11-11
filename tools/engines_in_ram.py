# PlatformIO extra script: run the drum engines from RAM.
# Each engine file is one translation unit; its code goes into .time_critical, which the
# RP2040 linker script copies to RAM at boot, so the voices never wait on the flash cache.
Import("env")

ENGINES = ("drum_engine.c", "hortator_engine.c")


def in_ram(node):
    if not node.name.endswith(ENGINES):
        return node
    obj = env.Object(node, CCFLAGS=env["CCFLAGS"] + ["-fno-function-sections"])
    env.AddPostAction(obj, env.VerboseAction(
        '"$OBJCOPY" --rename-section .text=.time_critical.drums $TARGET', "RAM code: $TARGET"))
    return obj


env.AddBuildMiddleware(in_ram, "*")
