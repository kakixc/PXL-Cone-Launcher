# Native PXL branding assets

Use the existing `program_info/PXLConeLauncher.icon/assets/logo/pxlcone-icon.svg` as the artwork source. Do not redraw or stretch it.

This optional developer tool renders a centered, transparent 1024 px PNG with the existing Qt SVG module. It is a separate CMake project and adds no launcher runtime dependency.

    cmake -S program_info/branding-tools -B build-branding -DCMAKE_PREFIX_PATH=<Qt install>
    cmake --build build-branding --config Release
    pxl-branding-render <source.svg> <output.png>

Package the PNG with Pillow as an ICO containing 16, 24, 32, 48, 64, 128 and 256 px frames. Save a 256 px PNG for the Linux icon. The active program_info ICO/SVG/PNG paths are selected by program_info/CMakeLists.txt. Keep the transparent border and aspect ratio. Validate the icon embedded in a newly built executable as well as these source assets.

    python program_info/branding-tools/package.py <output.png> program_info/pxlcone.ico program_info/pxlcone_256.png

Installed desktop/application IDs retain compatibility until the dedicated data-path migration; source branding assets use pxlcone filenames.
