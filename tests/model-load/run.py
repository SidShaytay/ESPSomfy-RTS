from pathlib import Path
import subprocess,sys,os,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'src/ConfigFile.cpp').read_text()+'\n'+(root/'src/ShadeController.cpp').read_text()
names=['ConfigFile::begin','ConfigFile::end','ConfigFile::readByte','ConfigFile::position','ConfigFile::seekRead','ConfigFile::readHeader','ShadeConfigFile::validate','ShadeConfigFile::loadBoot','ShadeConfigFile::loadFile','ShadeConfigFile::restoreFile','SomfyShadeController::begin']
with tempfile.TemporaryDirectory() as scratch:
    bodies=[]
    for name in names:
        at=source.index(name+'(');start=source.rfind('\n',0,at)+1;brace=source.index('{',at);end=brace+1;depth=1
        while depth:
            depth+=(source[end]=='{')-(source[end]=='}');end+=1
        bodies.append(source[start:end])
    Path(scratch,'production-load.inc').write_text('\n'.join(bodies))
    binary=str(Path(scratch,'load-test'))
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+scratch,'-I'+str(root/'test/stubs'),'-I'+str(root/'src'),str(root/'tests/model-load/load-test.cpp'),str(root/'src/ModelLoadSnapshot.cpp'),str(root/'src/ConfigRecords.cpp'),'-o',binary],check=True)
    subprocess.run([binary],check=True)
