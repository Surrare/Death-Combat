set(install_manifest "/home/dus/Softgays/Death-Combat/build-linux/install_manifest.txt")

if(NOT EXISTS "${install_manifest}")
    message(FATAL_ERROR "Install manifest not found: ${install_manifest}. Run cmake --install first.")
endif()

file(STRINGS "${install_manifest}" installed_files)
foreach(file IN LISTS installed_files)
    if(EXISTS "${file}" OR IS_SYMLINK "${file}")
        file(REMOVE "${file}")
        message(STATUS "Removed: ${file}")
    endif()
endforeach()
