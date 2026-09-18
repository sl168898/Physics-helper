Scriptname WPO_PlayerAlias extends ReferenceAlias

Event OnPlayerLoadGame()
    WPO_Controller controller = GetOwningQuest() as WPO_Controller
    If controller
        controller.Resume()
    EndIf
EndEvent

Event OnLocationChange(Location akOldLoc, Location akNewLoc)
    WPO_Controller controller = GetOwningQuest() as WPO_Controller
    If controller
        controller.ScanSoon()
    EndIf
EndEvent

Event OnCellLoad()
    WPO_Controller controller = GetOwningQuest() as WPO_Controller
    If controller
        controller.ScanSoon()
    EndIf
EndEvent
