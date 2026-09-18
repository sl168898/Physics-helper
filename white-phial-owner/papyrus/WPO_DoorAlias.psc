Scriptname WPO_DoorAlias extends ReferenceAlias

Function Enforce()
    WPO_Controller controller = GetOwningQuest() as WPO_Controller
    ObjectReference doorRef = GetReference()
    If controller && controller.IsRestricted() && doorRef
        ; Block default player activation even if the player possesses the shop key.
        ; Keep the original key and ownership for the NPCs' normal schedules.
        If !doorRef.IsActivationBlocked()
            doorRef.BlockActivation(True)
        EndIf
        If doorRef.GetLockLevel() != 255
            doorRef.SetLockLevel(255)
        EndIf
        If !doorRef.IsLocked()
            doorRef.Lock(True)
        EndIf
    EndIf
EndFunction

Event OnLockStateChanged()
    Enforce()
EndEvent

Event OnCellAttach()
    Enforce()
EndEvent

Event OnReset()
    Enforce()
EndEvent

Event OnActivate(ObjectReference akActionRef)
    WPO_Controller controller = GetOwningQuest() as WPO_Controller
    ObjectReference doorRef = GetReference()
    If !controller || !doorRef
        Return
    EndIf
    If controller.IsRestricted() && akActionRef == Game.GetPlayer()
        Enforce()
        Return
    EndIf
    If akActionRef
        ; Default-only processing bypasses our block without recursively sending OnActivate.
        doorRef.Activate(akActionRef, True)
    EndIf
EndEvent
