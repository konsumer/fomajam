// BLE MIDI test: find a peripheral whose name starts with a prefix, connect, subscribe to the MIDI I/O
// characteristic, send a few notes, and print everything that comes back.
//   bletest [name-prefix] [seconds]
import CoreBluetooth
import Foundation

let MIDI_SVC = CBUUID(string: "03B80E5A-EDE8-4B33-A751-6CE34EC4C700")
let MIDI_IO = CBUUID(string: "7772E5DB-3868-4112-A1A9-F2669D106BF3")
let args = CommandLine.arguments
let prefix = args.count > 1 ? args[1] : "FOMA"
let seconds = args.count > 2 ? Double(args[2]) ?? 30 : 30
let t0 = Date()

func log(_ s: String) {
  print(String(format: "%7.3f ", Date().timeIntervalSince(t0)) + s)
  fflush(stdout)
}
func hex(_ d: Data) -> String { d.map { String(format: "%02X", $0) }.joined(separator: " ") }

// one BLE-MIDI packet: header, then timestamp + message
func packet(_ msg: [UInt8]) -> Data {
  let ms = UInt16(Date().timeIntervalSince(t0) * 1000) & 0x1FFF
  return Data([0x80 | UInt8(ms >> 7), 0x80 | UInt8(ms & 0x7F)] + msg)
}

final class Tester: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
  var cm: CBCentralManager!
  var dev: CBPeripheral?
  var io: CBCharacteristic?
  var rx = 0

  override init() {
    super.init()
    cm = CBCentralManager(delegate: self, queue: nil)
  }

  func centralManagerDidUpdateState(_ c: CBCentralManager) {
    log("bluetooth state \(c.state.rawValue) (5 = on)")
    if c.state == .poweredOn {
      log("scanning for \"\(prefix)\"...")
      c.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }
  }

  func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData ad: [String: Any], rssi: NSNumber) {
    let name = (ad[CBAdvertisementDataLocalNameKey] as? String) ?? p.name ?? ""
    let svcs = (ad[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID]) ?? []
    if svcs.contains(MIDI_SVC) || name.hasPrefix(prefix) {
      log("found \"\(name)\" rssi \(rssi) midi-service \(svcs.contains(MIDI_SVC)) connectable \(ad[CBAdvertisementDataIsConnectable] ?? "?")")
    }
    guard dev == nil, name.hasPrefix(prefix) else { return }
    dev = p
    c.stopScan()
    p.delegate = self
    log("connecting")
    c.connect(p)
  }

  func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
    log("connected, discovering")
    p.discoverServices([MIDI_SVC])
  }
  func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
    log("connect failed: \(error?.localizedDescription ?? "?")")
    exit(2)
  }
  func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
    log("disconnected: \(error?.localizedDescription ?? "by us")")
  }

  func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
    guard let s = p.services?.first(where: { $0.uuid == MIDI_SVC }) else {
      log("no MIDI service: \(error?.localizedDescription ?? "")")
      exit(3)
    }
    p.discoverCharacteristics([MIDI_IO], for: s)
  }
  func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
    guard let ch = s.characteristics?.first(where: { $0.uuid == MIDI_IO }) else {
      log("no MIDI I/O characteristic")
      exit(4)
    }
    io = ch
    log("MIDI I/O found, properties \(ch.properties.rawValue); MTU \(p.maximumWriteValueLength(for: .withoutResponse))")
    p.setNotifyValue(true, for: ch)
  }
  func peripheral(_ p: CBPeripheral, didUpdateNotificationStateFor ch: CBCharacteristic, error: Error?) {
    log("notifications \(ch.isNotifying ? "on" : "off") \(error?.localizedDescription ?? "")")
    guard ch.isNotifying else { return }
    if args.count > 3 && args[3] == "listen" { log("listening only"); return }
    // a C major arpeggio, then the clock: start, 24 ticks, stop
    var at = 0.5
    for n: UInt8 in [60, 64, 67, 72] {
      DispatchQueue.main.asyncAfter(deadline: .now() + at) { self.send([0x90, n, 100], "note on \(n)") }
      DispatchQueue.main.asyncAfter(deadline: .now() + at + 0.3) { self.send([0x80, n, 0], "note off \(n)") }
      at += 0.4
    }
    DispatchQueue.main.asyncAfter(deadline: .now() + at) { self.send([0xFA], "start") }
    for i in 0..<24 {
      DispatchQueue.main.asyncAfter(deadline: .now() + at + 0.02 * Double(i + 1)) { self.send([0xF8], nil) }
    }
    DispatchQueue.main.asyncAfter(deadline: .now() + at + 0.6) { self.send([0xFC], "stop (24 ticks sent)") }
    DispatchQueue.main.asyncAfter(deadline: .now() + at + 1) { log("now play keys on the FM-1: they should show up below") }
  }
  func send(_ msg: [UInt8], _ what: String?) {
    guard let p = dev, let ch = io else { return }
    p.writeValue(packet(msg), for: ch, type: .withoutResponse)
    if let w = what { log("sent \(w)") }
  }
  func peripheral(_ p: CBPeripheral, didUpdateValueFor ch: CBCharacteristic, error: Error?) {
    guard let v = ch.value, !v.isEmpty else { return }
    rx += 1
    log("in: \(hex(v))")
  }
}

let t = Tester()
DispatchQueue.main.asyncAfter(deadline: .now() + seconds) {
  log("done: \(t.rx) notifications in")
  if let p = t.dev { t.cm.cancelPeripheralConnection(p) }
  exit(t.dev == nil ? 1 : 0)
}
RunLoop.main.run()
