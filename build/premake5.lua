-- ely-arcade-map-browser build script. Shared logic lives in the ely-arcade-sdk submodule (sdk/).
dofile("../sdk/premake/arcade_sdk.lua")

arcade.prepare_dirs()
arcade.workspace("ely-arcade-map-browser")
arcade.raylib_project()
arcade.sdk_project("../sdk")
arcade.app_project("ely-arcade-map-browser", "../src", "../sdk")