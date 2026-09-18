Scriptname WPO_TraitEffect extends ActiveMagicEffect
Quest Property Controller Auto

Event OnEffectStart(Actor akTarget, Actor akCaster)
    If akTarget != Game.GetPlayer() || !Controller
        Return
    EndIf
    ; Also starts the newly added quest when choosing this trait on a v2.0 save.
    If !Controller.IsRunning()
        If !Controller.Start()
            Debug.Trace("[White Phial Owner] Controller could not start.")
            Return
        EndIf
    EndIf
    (Controller as WPO_Controller).ScanSoon()
EndEvent
