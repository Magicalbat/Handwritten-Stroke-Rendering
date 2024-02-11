workspace "Line-Render-Test"
    configurations { "debug", "release" }
    startproject "Line-Render-Test"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Line-Render-Test"
    language "C"
    location "src"
    kind "ConsoleApp"

    includedirs {
        "src",
        "src/third_party"
    }

    files {
        "src/**.h",
        "src/**.c",
    }

    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    targetdir ("bin/" .. outputdir)
    targetprefix ""

    warnings "Extra"
    architecture "x64"
    toolset "clang"

    filter "system:linux"
        links {
            "m", "X11", "GL", "GLX",
        }

    filter { "system:windows", "action:*gmake*", "configurations:debug" }
        linkoptions { "-g" }

    filter "configurations:debug"
        symbols "On"
        defines { "DEBUG" }

    filter "configurations:release"
        optimize "On"
        defines { "NDEBUG" }

    filter "system:windows"
        systemversion "latest"

        links {
            "gdi32", "kernel32", "user32", "opengl32"
        }
