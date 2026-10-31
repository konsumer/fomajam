// BLE MIDI leader: advertise as a BLE MIDI peripheral ("BLETEST"), and once a central subscribes, send start and a
// MIDI clock at the given tempo; print what the central writes (its MIDI out).
//   leader [bpm] [seconds]
import CoreBluetooth
import Foundation

let MIDI_SVC = CBUUID(string: "03B80E5A-EDE8-4B33-A751-6CE34EC4C700")
let MIDI_IO = CBUUID(string: "7772E5DB-3868-4112-A1A9-F2669D106BF3")
let args = CommandLine.arguments
let bpm = args.count > 1 ? Double(args[1]) ?? 100 : 100
let seconds = args.count > 2 ? Double(args[2]) ?? 60 : 60
let t0 = Date()

func now() -> Double { Date().timeIntervalSince(t0) }
func log(_ s: String) {
  print(String(format: "%7.3f ", now()) + s)
  fflush(stdout)
}
func hex(_ d: Data) -> String { d.map { String(format: "%02X", $0) }.joined(separator: " ") }
func packet(_ msg: [UInt8]) -> Data {
  let ms = UInt16(now() * 1000) & 0x1FFF
  return Data([0x80 | UInt8(ms >> 7), 0x80 | UInt8(ms & 0x7F)] + msg)
}

final class Leader: NSObject, CBPeripheralManagerDelegate {
  var pm: CBPeripheralManager!
  var io: CBMutableCharacteristic!
  var subscribed = false
  var ticks = 0
  var writes = 0
  var timer: DispatchSourceTimer?

  override init() {
    super.init()
    pm = CBPeripheralManager(delegate: self, queue: nil)
  }

  func peripheralManagerDidUpdateState(_ p: CBPeripheralManager) {
    log("bluetooth state \(p.state.rawValue) (5 = on)")
    guard p.state == .poweredOn else { return }
    io = CBMutableCharacteristic(type: MIDI_IO, properties: [.read, .writeWithoutResponse, .notify],
                                 value: nil, permissions: [.readable, .writeable])
    let svc = CBMutableService(type: MIDI_SVC, primary: true)
    svc.characteristics = [io]
    p.add(svc)
  }
  func peripheralManager(_ p: CBPeripheralManager, didAdd service: CBService, error: Error?) {
    if let e = error { log("add service: \(e)"); exit(2) }
    p.startAdvertising([CBAdvertisementDataLocalNameKey: "BLETEST", CBAdvertisementDataServiceUUIDsKey: [MIDI_SVC]])
  }
  func peripheralManagerDidStartAdvertising(_ p: CBPeripheralManager, error: Error?) {
    log(error == nil ? "advertising as BLETEST, \(Int(bpm)) BPM once a central listens" : "advertise: \(error!)")
  }
  func peripheralManager(_ p: CBPeripheralManager, central: CBCentral, didSubscribeTo ch: CBCharacteristic) {
    log("central subscribed (MTU \(central.maximumUpdateValueLength)): start, then the clock")
    subscribed = true
    send([0xFA])
    let t = DispatchSource.makeTimerSource(queue: .main)
    t.schedule(deadline: .now() + 0.05, repeating: 60.0 / bpm / 24.0, leeway: .microseconds(200))
    t.setEventHandler { [weak self] in
      guard let s = self, s.subscribed else { return }
      s.send([0xF8])
      s.ticks += 1
      if s.ticks % (24 * 4) == 0 { log("\(s.ticks / 24) beats sent, \(s.writes) writes in") }
    }
    t.resume()
    timer = t
  }
  func peripheralManager(_ p: CBPeripheralManager, central: CBCentral, didUnsubscribeFrom ch: CBCharacteristic) {
    log("central unsubscribed")
    subscribed = false
  }
  func send(_ msg: [UInt8]) {
    if !pm.updateValue(packet(msg), for: io, onSubscribedCentrals: nil) { /* queue full: dropped (a tick) */ }
  }
  func peripheralManager(_ p: CBPeripheralManager, didReceiveRead r: CBATTRequest) {
    r.value = Data()
    p.respond(to: r, withResult: .success)
  }
  func peripheralManager(_ p: CBPeripheralManager, didReceiveWrite rs: [CBATTRequest]) {
    for r in rs {
      writes += 1
      if writes <= 12, let v = r.value { log("in: \(hex(v))") }
    }
  }
}

let l = Leader()
DispatchQueue.main.asyncAfter(deadline: .now() + seconds) {
  if l.subscribed { l.send([0xFC]) }
  log("done: \(l.ticks) ticks sent (\(l.ticks / 24) beats), \(l.writes) writes in")
  exit(l.subscribed || l.ticks > 0 ? 0 : 1)
}
RunLoop.main.run()
