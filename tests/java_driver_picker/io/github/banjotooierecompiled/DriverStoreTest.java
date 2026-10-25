package io.github.banjotooierecompiled;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.Arrays;

public final class DriverStoreTest {
    private static boolean failPublish;
    private static final DriverStore.FileOps OPS=new DriverStore.FileOps() {
        public void privateDirectory(File path) throws IOException {Files.setPosixFilePermissions(path.toPath(),PosixFilePermissions.fromString("rwx------"));}
        public void readOnly(File path) throws IOException {Files.setPosixFilePermissions(path.toPath(),PosixFilePermissions.fromString("r--------"));}
        public void replace(File from,File to) throws IOException {
            if(failPublish&&to.getName().equals("selection.txt")) throw new IOException("Deliberate publish failure");
            Files.move(from.toPath(),to.toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);
        }
    };
    interface Checked {void run() throws Exception;}
    static void rejected(Checked check) throws Exception {
        boolean stopped=false;try {check.run();} catch(IOException expected) {stopped=true;}
        if(!stopped) throw new AssertionError("Invalid driver was accepted");
    }
    public static void main(String[] args) throws Exception {
        File fixtures=new File(args[0]),files=new File(args[1]);assert files.mkdirs();
        DriverStore store=new DriverStore(files,31,OPS);assert !store.selected().custom();
        assert !store.setupSeen();store.markSetupSeen();assert store.setupSeen();
        assert new DriverStore(files,31,OPS).setupSeen();
        File setup=new File(store.root,"setup-seen.json");
        for(String invalid:new String[]{"{}","{\"schema\":2,\"seen\":true}","{\"schema\":1,\"seen\":false}","{\"schema\":1,\"seen\":true,\"seen\":true}"}) {
            Files.writeString(setup.toPath(),invalid);rejected(store::setupSeen);
        }
        setup.delete();Files.createSymbolicLink(setup.toPath(),new File(store.root,"missing-setup").toPath());
        rejected(store::setupSeen);store.markSetupSeen();assert store.setupSeen();
        DriverStore.Choice first;
        try(FileInputStream input=new FileInputStream(new File(fixtures,"good.zip"))) {first=store.importZip(input);}
        assert first.custom()&&first.minApi==28&&store.selected().fingerprint().equals(first.fingerprint());
        File library=new File(new File(store.root,first.directory),first.library);
        assert Files.getPosixFilePermissions(library.toPath()).equals(PosixFilePermissions.fromString("r--------"));
        byte[] selected=Files.readAllBytes(new File(store.root,"selection.txt").toPath());
        int cases=0;
        for(File bad:fixtures.listFiles((parent,name)->name.startsWith("bad-")&&name.endsWith(".zip"))) {
            rejected(()->{try(FileInputStream input=new FileInputStream(bad)) {store.importZip(input);}});
            assert Arrays.equals(selected,Files.readAllBytes(new File(store.root,"selection.txt").toPath())):bad.getName();cases++;
        }
        assert cases>=15;
        failPublish=true;
        rejected(()->{try(FileInputStream input=new FileInputStream(new File(fixtures,"good.zip"))) {store.importZip(input);}});
        failPublish=false;assert Arrays.equals(selected,Files.readAllBytes(new File(store.root,"selection.txt").toPath()));
        assert store.choices().size()==2;assert library.isFile();
        store.reset();assert !store.selected().custom()&&library.isFile()&&store.setupSeen();
        store.select(first);assert store.selected().fingerprint().equals(first.fingerprint());
        File data=new File(files,"save-fixture.bin");Files.write(data.toPath(),new byte[]{1,2,3});store.reset();assert data.length()==3;
        File selection=new File(store.root,"selection.txt");Files.createSymbolicLink(selection.toPath(),new File(store.root,"missing").toPath());
        rejected(store::selected);store.reset();assert !store.selected().custom();
        store.select(first);
        File other=new File(files,"outside");assert other.mkdir();
        rejected(()->store.atomicWrite(new File(other,"selection.txt"),"no"));
        DriverStore copied=new DriverStore(files,31,OPS);assert copied.selected().fingerprint().equals(first.fingerprint())&&copied.setupSeen();
        System.out.println("PASS: actual driver importer/store, "+cases+" invalid packages, failed publish, reset, retained files and restored selection");
        System.out.println("NATIVE_ROOT="+store.root.getAbsolutePath());
    }
}
