"""Add the localized Y prompt to the installed game's car-details layout."""

import sys
from pathlib import Path
from zipfile import ZipFile

anchor = 'local:ControllerButtonPanel.XDescription="{local:Loc HelpButtons.IDS_ChangeCamera}"'
layouts = {"CarDetails": "kCarDetailsLayout", "ViewCar": "kViewCarLayout"}
headers = []
with ZipFile(sys.argv[1]) as archive:
    for scene, variable in layouts.items():
        layout = archive.read(f"Scenes/Anthem/A_{scene}.xaml").decode("utf-8-sig")
        layout = layout.replace("\r\n", "\n").replace("\r", "\n")
        if layout.count(anchor) != 1 or "ControllerButtonPanel.YDescription" in layout:
            raise ValueError(f"Unexpected button bar in A_{scene}.xaml")
        layout = layout.replace(
            anchor,
            anchor + '\n                    local:ControllerButtonPanel.YDescription="{local:Loc HelpButtons.IDS_Explode}"',
        )
        headers.append(f'constexpr char {variable}[] = R"FH2XAML({layout})FH2XAML";\n')
Path(sys.argv[2]).write_text("".join(headers), encoding="utf-8")
