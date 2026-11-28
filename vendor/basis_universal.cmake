if(TARGET mbgl-vendor-basis_universal)
    return()
endif()

add_library(
    mbgl-vendor-basis_universal STATIC
    ${CMAKE_CURRENT_LIST_DIR}/basis_universal/transcoder/basisu_transcoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/basis_universal/zstd/zstddeclib.c
)

target_include_directories(
    mbgl-vendor-basis_universal SYSTEM
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/basis_universal/transcoder
)

# Only KTX2's ETC1S and UASTC, transcoded to ASTC or RGBA: the photos' formats.
target_compile_definitions(
    mbgl-vendor-basis_universal
    PUBLIC
        BASISD_SUPPORT_KTX2=1
        BASISD_SUPPORT_KTX2_ZSTD=1
        BASISD_SUPPORT_DXT1=0
        BASISD_SUPPORT_DXT5A=0
        BASISD_SUPPORT_PVRTC1=0
        BASISD_SUPPORT_PVRTC2=0
        BASISD_SUPPORT_ETC2_EAC_A8=0
        BASISD_SUPPORT_ETC2_EAC_RG11=0
        BASISD_SUPPORT_ATC=0
        BASISD_SUPPORT_FXT1=0
)

target_compile_options(mbgl-vendor-basis_universal PRIVATE -w)

set_target_properties(
    mbgl-vendor-basis_universal
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERFACE_MAPLIBRE_NAME "basis_universal"
        INTERFACE_MAPLIBRE_URL "https://github.com/BinomialLLC/basis_universal"
        INTERFACE_MAPLIBRE_AUTHOR "Binomial LLC"
        INTERFACE_MAPLIBRE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/basis_universal/LICENSE
)
