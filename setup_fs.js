/* Mounts an IDBFS (indexedDb) directory for save files.
The directory's name should match what is used by generate_respawn_filename
to generate save file names. */

Module.preRun.push(function() {
    FS.mkdir("saves");
    FS.mount(IDBFS, {}, "saves");
});
