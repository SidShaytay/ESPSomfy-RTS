#include "LiftGuideView.h"
LiftGuideView liftGuideView(LiftGuideStage stage, bool applied, bool durable, bool reversalsKnown) {
  using S=LiftGuideStage; using A=LiftGuideAction;
  LiftGuideView view;
  auto add=[&](A action,const char *label){view.choices[view.count++]={action,label};};
  bool measuring=false;
  switch(stage) {
    case S::reference:
      view.title="Confirm fully open";view.hint="Press Open. Wait until fully open and motor silent.";
      view.instructions="Stand where you can see the whole cover. Press Open fully, then wait until it is completely open and the motor has stopped. For curtains, open means fully apart; for an awning, fully retracted. Confirm only what you can see. This wizard measures movement; it does not change motor limits.";
      add(A::open,"Open fully");add(A::confirmOpen,"It is fully open");measuring=true;break;
    case S::readyDown:
      view.title="Measure closing";view.hint="Watch the moving edge. Next: close it all the way.";
      view.instructions="We will measure a complete closing movement. Watch the leading edge (the bottom edge on a shutter or roller blind). Be ready to mark halfway across the opening by distance, not halfway through the time. Press Start closing when ready.";
      add(A::startDown,"Start closing");break;
    case S::readyUp:
      view.title="Measure opening";view.hint="Now watch a full opening movement.";
      view.instructions="Now measure a complete opening movement in the other direction. Watch the same edge. Again, halfway means half the opening by distance. Press Start opening when ready.";
      add(A::startUp,"Start opening");break;
    case S::startingDown:case S::startingUp:
      view.title="Starting movement";view.hint="Wait for the next instruction. Watch the cover.";measuring=true;break;
    case S::downTakeup:case S::upTakeup:
      view.title=stage==S::downTakeup?"Closing: first movement":"Opening: first movement";
      view.hint="Press as soon as the main moving edge starts moving.";
      view.instructions="Watch the main moving edge, not just the motor or individual gaps between slats. Press The edge started moving as soon as that edge moves. Any motor movement before that is measured automatically as slack. If movement starts immediately, press immediately.";
      add(A::moving,"The edge started moving");measuring=true;break;
    case S::downHalf:case S::upHalf:
      view.title=stage==S::downHalf?"Closing: halfway":"Opening: halfway";
      view.hint="Press at half the opening by distance, not time.";
      view.instructions="Press The edge is halfway when the moving edge is halfway by distance between the fully open and fully closed positions. Use the actual opening as your reference; do not use the displayed percentage or count seconds. The cover keeps moving after you press.";
      add(A::halfway,"The edge is halfway");measuring=true;break;
    case S::downEnd:case S::upEnd:
      view.title=stage==S::downEnd?"Closing: edge at end":"Opening: edge at end";
      view.hint="Press when the edge reaches the end of its travel.";
      view.instructions="Press The edge reached the end when the main moving edge reaches its final position. On a slatted shutter, gaps may still be closing or opening and the motor may still run: that is measured in the next step. On a curtain or awning, use its fully closed or open edge position.";
      add(A::visibleEnd,"The edge reached the end");measuring=true;break;
    case S::downConfirm:case S::upConfirm:
      view.title=stage==S::downConfirm?"Confirm fully closed":"Confirm fully open";
      view.hint="Wait for all movement AND motor to stop. Then press.";
      view.instructions="Wait until the cover is completely at the requested end, all visible movement has finished, and the motor has stopped. If it has slats, wait until every gap stops changing. Then press Fully at the end; motor stopped promptly. Do not press while the motor is running. This confirms a real endpoint, not an estimate.";
      add(A::confirmEndpoint,"Fully at end; motor stopped");measuring=true;break;
    case S::review:
      view.title="Review calibration";view.hint=reversalsKnown?"Save, then check positions while watching.":"Next: check delay when changing direction.";
      view.instructions=reversalsKnown?"Both full movements and any direction-change delay have been measured. Save to use these measurements for this device and its groups. Then check 25%, 50% and 75% while watching, in both directions. If you missed a marker, discard and start again.":"One more check: the motor may move briefly before the cover moves after changing direction. Press Check direction changes. The wizard will move towards halfway and ask you to stop it there, then measure both direction changes. You do not need to decide whether there is slack.";
      if(reversalsKnown) add(A::save,"Save calibration");
      else add(A::measureReversals,"Check direction changes");
      break;
    case S::reversePosition:
      view.title="Move to halfway";view.hint="Press Stop at halfway across the opening.";
      view.instructions="The cover is closing. Press Halfway: stop here when the moving edge reaches halfway across the opening. This stops the cover so we can check what happens when direction changes.";
      add(A::stopAtHalf,"Halfway: stop here");measuring=true;break;
    case S::reversePositionStopping:case S::reverseStoppingUp:case S::reverseStoppingDown:
      view.title="Waiting for Stop";view.hint="Stopping. Wait for the next instruction.";break;
    case S::reverseReadyUp:case S::reverseReadyDown:
      view.title="Change direction";view.hint="Press to reverse, then watch for the edge to move.";
      view.instructions="Press the direction button below. Watch the main moving edge closely. On the next screen, press as soon as it starts moving. The wizard will stop it automatically; any delay is recorded, even when it is very small.";
      add(stage==S::reverseReadyUp?A::startReverseUp:A::startReverseDown,
        stage==S::reverseReadyUp?"Reverse towards open":"Reverse towards closed");break;
    case S::reverseStartingUp:case S::reverseStartingDown:
      view.title="Starting direction change";view.hint="Watch the edge. Wait for the next instruction.";measuring=true;break;
    case S::reverseTakeupUp:case S::reverseTakeupDown:
      view.title="Watch the moving edge";view.hint="Press at first edge movement. We stop it for you.";
      view.instructions="Press The edge started moving as soon as the main moving edge moves in the new direction, even if it moves immediately. The wizard sends Stop automatically after your press. Ignore motor noise or gaps changing before the main edge moves.";
      add(A::moving,"The edge started moving");measuring=true;break;
    case S::reverseFinish:
      view.title="Confirm fully open";view.hint="Press Open. Confirm when fully open and motor silent.";
      view.instructions="The direction-change checks are complete. Press Open fully to return to a known position. Wait until the cover is completely open and the motor stops, then confirm. After this you can save the calibration.";
      add(A::open,"Open fully");add(A::confirmOpen,"It is fully open");measuring=true;break;
    case S::saving:view.title="Saving calibration";view.hint="Please wait. Saving your measurements.";return view;
    case S::complete:
      view.title="Calibration saved";view.hint="Test 25/50/75%, then test the group.";
      add(A::cancel,"Close");return view;
    case S::failed:
      view.title=applied&&!durable?"Applied; save failed":"Calibration interrupted";
      view.hint=applied&&!durable?"Running profile changed. Retry COMMIT in storage recovery.":
        "Draft lost; movement may continue.";
      add(A::cancel,"Close");return view;
    case S::closed:return view;
  }
  if(measuring)add(A::stop,"Stop movement");
  add(A::cancel,"Discard draft");
  return view;
}
