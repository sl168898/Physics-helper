Scriptname WPD_PlayerAlias extends ReferenceAlias

Spell Property DecantPower Auto
GlobalVariable Property AutoDecantAt8AM Auto
GlobalVariable Property FullyEnchanted Auto

Bool powerUnlocked = False
Bool autoWasEnabled = False
Float nextMorning = -1.0
Float previousTime = -1.0
Int pendingAttempts = 0
Bool storageChecked = False

Event OnInit()
    RegisterForSingleUpdate(1.0)
EndEvent

Event OnPlayerLoadGame()
    storageChecked = False
    ; Do not resume retries across a load; the reserved morning stays saved.
    pendingAttempts = 0
    RegisterForSingleUpdate(1.0)
EndEvent

Event OnUpdate()
    Actor player = GetActorReference()
    WPD_DecantQuest controller = GetOwningQuest() as WPD_DecantQuest
    If player && controller && !Utility.IsInMenuMode()
        ; Existing v1 saves may not populate newly introduced VMAD properties.
        If !AutoDecantAt8AM
            AutoDecantAt8AM = Game.GetFormFromFile(0x803, "White Phial - Decanting.esp") as GlobalVariable
        EndIf
        If !FullyEnchanted
            FullyEnchanted = Game.GetFormFromFile(0x80D, "The White Phial - Tweaks and Enhancements.esp") as GlobalVariable
        EndIf
        If !storageChecked
            storageChecked = True
            controller.ProtectCurrentLiquid()
        EndIf
        UpdatePower(player, controller)
        UpdateDailyDecant(player, controller, Utility.GetCurrentGameTime())
    EndIf
    ; Game time determines the deadline; real time only drives checks.
    RegisterForSingleUpdate(1.0)
EndEvent

Function UpdatePower(Actor player, WPD_DecantQuest controller)
    If !DecantPower
        Return
    EndIf
    If !powerUnlocked
        If FullyEnchanted && FullyEnchanted.GetValue() == 1.0 && controller.HasFullPhial(player)
            powerUnlocked = True
        Else
            ; Migrate v1's unconditional grant without granting quest progress.
            If player.HasSpell(DecantPower)
                player.RemoveSpell(DecantPower)
            EndIf
            Return
        EndIf
    EndIf
    If !player.HasSpell(DecantPower)
        player.AddSpell(DecantPower, False)
    EndIf
EndFunction

Float Function FollowingMorning(Float currentTime)
    Float morning = (currentTime as Int) + (8.0 / 24.0)
    If morning <= currentTime
        morning += 1.0
    EndIf
    Return morning
EndFunction

Function UpdateDailyDecant(Actor player, WPD_DecantQuest controller, Float currentTime)
    Bool enabled = AutoDecantAt8AM && AutoDecantAt8AM.GetValue() == 1.0
    If !enabled
        autoWasEnabled = False
        nextMorning = -1.0
        previousTime = currentTime
        pendingAttempts = 0
        Return
    EndIf
    If !autoWasEnabled || nextMorning < 0.0 || currentTime < previousTime
        ; Enabling after 8 AM schedules tomorrow, without a catch-up dose.
        ; Rewinding the clock starts a fresh future deadline as well.
        autoWasEnabled = True
        nextMorning = FollowingMorning(currentTime)
        previousTime = currentTime
        pendingAttempts = 0
        Return
    EndIf
    previousTime = currentTime
    If currentTime >= nextMorning
        ; Reserve this morning BEFORE touching inventory. Long waits/travel
        ; get one check, never a loop granting doses for missed days.
        nextMorning = FollowingMorning(currentTime)
        pendingAttempts = 3
    EndIf
    If pendingAttempts > 0
        pendingAttempts -= 1
        ; Short retries allow a refill also due at 8 AM to finish its alias swap.
        If controller.AutoDecant(player)
            pendingAttempts = 0
        EndIf
    EndIf
EndFunction
