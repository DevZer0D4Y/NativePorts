import GameController
@main struct PortGamepadTests {
 static func main() {
  let count = Int(CommandLine.arguments[1])!
  let controllers = (0..<count).map { _ in GCController.withExtendedGamepad() }
  var connected = controllers
  let hub = GamepadHub(controllerProvider: { connected })
  _ = hub.poll(now: 0)
  var now = 1.0
  for (slot, controller) in controllers.enumerated() {
   let g = controller.extendedGamepad!
   for (action, buttons) in PadButton.defaults {
    for button in buttons {
     guard let input = button.input(in: g) else { continue }
     input.setValue(1)
     g.leftThumbstick.xAxis.setValue(-1)
     let sample = hub.poll(now: now)
     assert(sample.players[slot].held.contains(action))
     assert(sample.players[slot].directions == Pad.left)
     assert(sample.players.enumerated().allSatisfy { $0.offset == slot || $0.element.held.isEmpty })
     assert(controller.playerIndex.rawValue == GamepadHub.playerOrder[slot])
     input.setValue(0); g.leftThumbstick.xAxis.setValue(0)
     _ = hub.poll(now: now + 0.01); now += 0.1
    }
   }
   g.buttonMenu.setValue(1); _ = hub.poll(now: now)
   g.buttonMenu.setValue(0); assert(hub.poll(now: now + 0.1).players[slot].startTap)
   now += 1
  }
  connected.reverse()
  let action = Action.shoot
  let button = PadButton.defaults[action]!.first!
  let input = button.input(in: controllers[1].extendedGamepad!)!
  input.setValue(1)
  assert(hub.poll(now: now).players[1].held.contains(action))
  hub.suppressHeldButtons()
  assert(hub.poll(now: now + 0.1).players[1].held.isEmpty)
  input.setValue(0); _ = hub.poll(now: now + 0.2)
  connected.removeAll { $0 === controllers[0] }
  assert(hub.poll(now: now + 0.3).players[0].held.isEmpty)
  let replacement = GCController.withExtendedGamepad(); connected.append(replacement)
  replacement.extendedGamepad!.buttonMenu.setValue(1); _ = hub.poll(now: now + 1)
  assert(hub.poll(now: now + 1.7).events.contains(.pause))
  replacement.extendedGamepad!.buttonMenu.setValue(0)
  assert(!hub.poll(now: now + 1.8).players[0].startTap)
  hub.assign(.rb, to: action)
  replacement.extendedGamepad!.rightShoulder.setValue(1)
  assert(hub.poll(now: now + 2).players[0].held.contains(action))
  print("PASS: \(count)-player Apple controller frontend: all default bindings, directions, Start/Pause, stable slots, reconnect, remapping.")
 }
}
