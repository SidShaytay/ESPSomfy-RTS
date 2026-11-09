from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
parts={'MqttConnection.cpp':['MqttConnection::reset','MqttConnection::loop','MqttConnection::connect','MqttConnection::disconnect','MqttConnection::connected','MqttConnection::receive','MqttConnection::subscribe','MqttConnection::unsubscribe','MqttConnection::publish(const char *topic, const char *payload','MqttConnection::unpublish','MqttConnection::publishBuffer','MqttConnection::publishDisco'], 'outputs/MqttPublisher.cpp':['SomfyShadeController::publish'], 'outputs/SomfyEvents.cpp':['SomfyShadeController::sendChanges']}
with tempfile.TemporaryDirectory() as scratch:
 bodies=[]
 for filename,names in parts.items():
  source=(root/'src'/filename).read_text()
  for name in names:
   at=source.index(name if '(' in name else name+'(');start=source.rfind('\n',0,at)+1;brace=source.index('{',at);end=brace+1;depth=1
   while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
   bodies.append(source[start:end])
 Path(scratch,'production-mqtt.inc').write_text('\n'.join(bodies))
 binary=str(Path(scratch,'mqtt'))
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-I'+scratch,'-I'+str(root/'tests/mqtt-trust'),'-I'+str(root/'.pio/libdeps/native/ArduinoJson/src'),'-I'+str(root/'test/stubs'),'-I'+str(root/'src'),str(root/'tests/mqtt-trust/test.cpp'),'-o',binary],check=True)
 subprocess.run([binary],check=True)
