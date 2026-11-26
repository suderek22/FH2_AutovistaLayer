"""Add the localized Y prompt to the installed game's car-details layout."""

import sys
from pathlib import Path
from zipfile import ZipFile

with ZipFile(sys.argv[1]) as archive:
    layout = archive.read("Scenes/Anthem/A_CarDetails.xaml").decode("utf-8-sig")
    layout = layout.replace("\r\n", "\n").replace("\r", "\n")

anchor = 'local:ControllerButtonPanel.XDescription="{local:Loc HelpButtons.IDS_ChangeCamera}"'
assert layout.count(anchor) == 1
assert "ControllerButtonPanel.YDescription" not in layout
layout = layout.replace(
    anchor,
    anchor + '\n                    local:ControllerButtonPanel.YDescription="{local:Loc HelpButtons.IDS_Explode}"',
)
Path(sys.argv[2]).write_text(
    'constexpr char kCarDetailsLayout[] = R"FH2XAML(' + layout + ')FH2XAML";\n',
    encoding="utf-8",
)
