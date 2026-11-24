import unreal

lvl = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if lvl:
    ok = lvl.load_level("/Game/Battle/Maps/Lvl_ConcreteArena")
    unreal.log("LOAD_LEVEL %s" % ok)

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors() if sub else []
unreal.log("TOTAL %d" % len(actors))
for a in actors:
    n = a.get_class().get_name()
    unreal.log("ACTOR %s | %s | loc=%s" % (n, a.get_actor_label(), a.get_actor_location()))
