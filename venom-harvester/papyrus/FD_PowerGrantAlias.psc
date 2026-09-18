Scriptname FD_PowerGrantAlias extends ReferenceAlias
Spell Property TraitAbility Auto
Spell Property PledgePower Auto
GlobalVariable Property Activated Auto
GlobalVariable Property MenuBusy Auto
GlobalVariable Property FavorCap Auto
GlobalVariable Property PreviousCap Auto
GlobalVariable Property PreviousTarget Auto
WSN_TrackerQuest_Quest Property Tracker Auto

Event OnInit()
    RegisterForSingleUpdate(1.0)
EndEvent

Event OnPlayerLoadGame()
    MenuBusy.SetValue(0.0)
    RegisterForSingleUpdate(1.0)
EndEvent

Event OnUpdate()
    Actor player = Game.GetPlayer()
    Spell penalty = Game.GetFormFromFile(0x830, "Biggie Traits - Fully Devoted.esp") as Spell
    Bool hasTrait = player.HasSpell(TraitAbility)
    MigrateVenomHarvester(player)
    ; Retire the old activation power on existing saves.
    If player.HasSpell(PledgePower)
        player.RemoveSpell(PledgePower)
    EndIf
    If hasTrait && Activated.GetValue() != 1.0
        If FavorCap.GetValue() != 400.0
            PreviousCap.SetValue(FavorCap.GetValue())
        EndIf
        If Tracker.WSN_FavoredDiminishTarget != 400.0
            PreviousTarget.SetValue(Tracker.WSN_FavoredDiminishTarget)
        EndIf
        Activated.SetValue(1.0)
    EndIf
    If hasTrait && Activated.GetValue() == 1.0
        FavorCap.SetValue(400.0)
        Tracker.WSN_FavoredDiminishTarget = 400.0
        If penalty && !player.HasSpell(penalty)
            player.AddSpell(penalty, False)
        EndIf
    Else
        If penalty && player.HasSpell(penalty)
            player.RemoveSpell(penalty)
        EndIf
    EndIf
    If !hasTrait && Activated.GetValue() != 0.0
        Activated.SetValue(0.0)
        FavorCap.SetValue(PreviousCap.GetValue())
        Tracker.FDRemoveTrait(PreviousTarget.GetValue())
    EndIf
    RegisterForSingleUpdate(0.5)
    VH_Native.Poll()
EndEvent

Function GrantClothingOnce(Actor player)
    ; Retained as an inert legacy function for existing saves.
    Return
EndFunction

Function GrantAlchemyGiftOnce(Actor player)
    ; Retained as an inert legacy function for existing saves.
    Return
EndFunction

Function MigrateVenomHarvester(Actor player)
    GlobalVariable version = Game.GetFormFromFile(0x805, "Biggie Traits - Devoted Alchemist.esp") as GlobalVariable
    If !version || version.GetValue() >= 2.0
        Return
    EndIf
    Spell ability = Game.GetFormFromFile(0x800, "Biggie Traits - Devoted Alchemist.esp") as Spell
    Perk oldPerk = Game.GetFormFromFile(0x802, "Biggie Traits - Devoted Alchemist.esp") as Perk
    GlobalVariable gifts = Game.GetFormFromFile(0x804, "Biggie Traits - Devoted Alchemist.esp") as GlobalVariable
    If !ability || !oldPerk || !gifts
        Return
    EndIf
    gifts.SetValue(1.0)
    If version.GetValue() == 0.0
        If player.HasSpell(ability)
            ; Save selection before removing the cached old ability.
            version.SetValue(1.0)
        Else
            If player.HasPerk(oldPerk)
                player.RemovePerk(oldPerk)
            EndIf
            version.SetValue(2.0)
            Return
        EndIf
    EndIf
    If version.GetValue() == 1.0
        If player.HasSpell(ability)
            If !player.RemoveSpell(ability)
                Return
            EndIf
        EndIf
        ; Remove the old saved disease effect as well as the retired perk.
        player.DispelSpell(ability)
        If player.HasPerk(oldPerk)
            player.RemovePerk(oldPerk)
        EndIf
        player.AddSpell(ability, False)
        If player.HasSpell(ability)
            version.SetValue(2.0)
        EndIf
    EndIf
EndFunction
