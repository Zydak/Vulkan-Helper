workspace "Vulture"
    configurations { "Debug", "Release", "Dist" }
    platforms { "Windows", "Linux" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
	
include "Vulture"
include "Sandbox"
