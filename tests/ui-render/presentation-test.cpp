#include "LocalUi.h"
#include <assert.h>
using namespace LocalUi;
#include "production-local-presentation.inc"
int main() {
  View shown,current;shown.entity=current.entity={EntityKind::device,7};
  shown.openedToken=current.openedToken=11;shown.action=current.action=Action::setFavouriteHere;
  for(auto screen:{Screen::favouriteObserve,Screen::favouriteConfirm}) {
    shown.screen=current.screen=screen;shown.favouriteRevision=current.favouriteRevision=31;
    assert(sameMeaning(current,shown,1,1,11,11));
    ++current.favouriteRevision;assert(!sameMeaning(current,shown,1,1,11,11));
    current.favouriteRevision=31;current.valueChoice=1;assert(!sameMeaning(current,shown,1,1,11,11));current.valueChoice=0;
  }
  shown.screen=current.screen=Screen::repeatValue;shown.repeatStep=current.repeatStep=2;
  assert(sameMeaning(current,shown,1,1,11,11));++current.repeatStep;
  assert(!sameMeaning(current,shown,1,1,11,11));
  shown.screen=current.screen=Screen::control;shown.repeatHeld=current.repeatHeld=false;
  assert(sameMeaning(current,shown,1,1,11,11));current.repeatHeld=true;
  assert(!sameMeaning(current,shown,1,1,11,11));
}
