#include "LiftGuideView.h"
LiftGuideView liftGuideView(LiftGuideStage stage, bool applied, bool durable, bool reversalsKnown) {
  using S=LiftGuideStage; using A=LiftGuideAction;
  LiftGuideView view;
  auto add=[&](A action,const char *label){view.choices[view.count++]={action,label};};
  bool measuring=false;
  switch(stage) {
    case S::reference:
      view.title="Confirm fully open";view.hint="Confirm after the motor stops.";
      add(A::open,"Open shutter");add(A::confirmOpen,"It is fully open");measuring=true;break;
    case S::readyDown:
      view.title="Measure closing";view.hint="Measure, or discard to keep settings.";
      add(A::startDown,"Start closing");break;
    case S::readyUp:
      view.title="Measure opening";view.hint="Measure the other direction.";
      add(A::startUp,"Start opening");break;
    case S::startingDown:case S::startingUp:
      view.title="Waiting for RF start";view.hint="Timing starts at RF transmission.";measuring=true;break;
    case S::downTakeup:case S::upTakeup:
      view.title=stage==S::downTakeup?"Closing: take-up":"Opening: take-up";
      view.hint="Mark the first visible movement.";
      add(A::moving,"Visible movement began");measuring=true;break;
    case S::downHalf:case S::upHalf:
      view.title=stage==S::downHalf?"Closing: halfway":"Opening: halfway";
      view.hint="Mark halfway through visible travel.";
      add(A::halfway,"At halfway");measuring=true;break;
    case S::downEnd:case S::upEnd:
      view.title=stage==S::downEnd?"Closing: visible end":"Opening: visible end";
      view.hint="Mark visible end, before settling.";
      add(A::visibleEnd,"Visible travel ended");measuring=true;break;
    case S::downConfirm:case S::upConfirm:
      view.title=stage==S::downConfirm?"Confirm fully closed":"Confirm fully open";
      view.hint="Wait for gaps and motor to stop.";
      add(A::confirmEndpoint,"Endpoint reached; stopped");measuring=true;break;
    case S::review:
      view.title="Review calibration";view.hint="Review both directions before saving.";
      if(reversalsKnown) add(A::save,"Save calibration");
      else { add(A::measureReversals,"Measure reversal play"); add(A::confirmNoReversal,"Confirmed no reversal play"); }
      break;
    case S::reversePosition:
      view.title="Position for reversal";view.hint="Stop when the shutter reaches halfway.";
      add(A::stopAtHalf,"At halfway: Stop");measuring=true;break;
    case S::reversePositionStopping:case S::reverseStoppingUp:case S::reverseStoppingDown:
      view.title="Waiting for Stop";view.hint="Wait for the full RF press to finish.";break;
    case S::reverseReadyUp:case S::reverseReadyDown:
      view.title="Measure reversal play";view.hint="Mark first movement after reversal.";
      add(stage==S::reverseReadyUp?A::startReverseUp:A::startReverseDown,
        stage==S::reverseReadyUp?"Reverse upward":"Reverse downward");break;
    case S::reverseStartingUp:case S::reverseStartingDown:
      view.title="Waiting for reversal RF";view.hint="Timing starts at RF transmission.";measuring=true;break;
    case S::reverseTakeupUp:case S::reverseTakeupDown:
      view.title="Reversal: take-up";view.hint="Mark first movement; then it stops.";
      add(A::moving,"Visible movement began");measuring=true;break;
    case S::reverseFinish:
      view.title="Confirm fully open";view.hint="Open fully; confirm after motor stops.";
      add(A::open,"Open shutter");add(A::confirmOpen,"It is fully open");measuring=true;break;
    case S::saving:view.title="Saving calibration";view.hint="Waiting for the settings file result.";return view;
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
  if(measuring)add(A::stop,"Stop moving shutter");
  add(A::cancel,"Discard draft");
  return view;
}
