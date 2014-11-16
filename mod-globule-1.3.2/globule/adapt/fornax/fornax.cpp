/*
Copyright (c) 2003-2006, Vrije Universiteit
All rights reserved.

Redistribution and use of the GLOBULE system in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.

   * Neither the name of Vrije Universiteit nor the names of the software
     authors or contributors may be used to endorse or promote
     products derived from this software without specific prior
     written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL VRIJE UNIVERSITEIT OR ANY AUTHORS OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This product includes software developed by the Apache Software Foundation
<http://www.apache.org/>.
*/
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <limits.h>
#include "fornax.h"

namespace fornax {

using namespace std;

/****************************************************************************/

void
StateBase::cancel(StateBase* state)
{
  BUG(cerr << "State::cancel; [" << this << "/" << name()
           << "] substates except [" << state << "/" << state->name()
           << "]" << endl);
  for(StateBase* aux, *run=firstcondition; run; run=aux) {
    aux = run->nextcondition;
    if(run != state) {
      assert(run->isblocked());
      assert((run->firstcondition ? run->firstcondition->listener == run
                                  : run->time == -1));
      run->cancel();
      delete run;
    } else {
      run->nextcondition = 0;
    }
  }
  firstcondition = state;
  condition = state;
  assert(!state->nextcondition);
  assert(state->listener == this);
}

/****************************************************************************/

StateBase::StateBase(System* s)
  : _blocked(false), system(s), condition(0), left(0), right(0), parent(0),
    firstcondition(0), nextcondition(0)
{
  system->statesActive.append(this);
}

StateBase::StateBase(System* s, Time t)
  : _blocked(false), system(s), condition(0), left(0), right(0), parent(0),
    firstcondition(0), nextcondition(0), time(t)
{
  if(t > 0)
    system->statesWaiting.insert(this);
  else
    system->statesActive.append(this);
}

StateId StateBase::error()
{
  cerr << "FAILURE" << endl;
  terminate();
  return 0;
}

#ifdef NAMEDSTATES
string StateBase::name() const
{
  return "<unknown state-base>";
}
#endif

std::ostream& StateBase::output(std::ostream& ost) const
{
  return ost;
}

/****************************************************************************/

//  QueueBase::QueueBase(string name, int subroutine)
//    : Blocker(), _name(name), stateid(subroutine)
//  {
//  }
QueueBase::QueueBase(EntityBase* entity, string name, int subroutine)
  : Blocker(), _name(name), stateid(subroutine)
{
  entity->queues.push_back(this);
  owner = entity;
}

std::ostream& QueueBase::output(std::ostream& ost) const
{
  return ost;
}

std::ostream& operator<< (std::ostream& ost, const QueueBase& queue)
{
  return queue.output(ost);
}

/****************************************************************************/

EntityBase::EntityBase(System& environment)
  : _name("<unknown>"), system(environment)
{
  system.add(this);
}

EntityBase::EntityBase(System& environment, string name)
  : _name(name), system(environment)
{
  system.add(this);
}

void EntityBase::terminate() {
  system.terminate();
}

std::ostream& EntityBase::output(std::ostream& ost) const {
  return ost;
}

std::ostream& operator<< (std::ostream& ost, const EntityBase& entity) {
  return entity.output(ost);
}

void EntityBase::recv(StateBase* state, QueueBase& q) {
  StateBase* spawn = state->pushstate(q.stateid NAMEDARG(q.name()));
  q.waitfor(spawn);
  spawn->waitfor(state);
}

void EntityBase::recv(StateBase* state,
                      const int nqs, QueueBase** qs)
{
  int i;
  BUG(cerr << "EntityBase::recv; [" << this << "] "
           << "multiple queue wait num=" << nqs << endl);
  for(i=0; i<nqs; i++)
    if(!qs[i]->empty()) {
      BUG(cerr << "EntityBase::recv; [" << this << "] "
               << "queue " << i << " named " << qs[i]->_name
               << " already contains event" << endl);
      return recv(state, *qs[i]);
    }
  StateBase* spawned;
  for(i=0; i<nqs; i++) {
    spawned = state->pushstate(qs[i]->stateid NAMEDARG(qs[i]->name()));
    BUG(cerr << "EntityBase::recv; [" << this << "] "
             << "pushed state " << spawned << endl);
    qs[i]->waitfor(spawned);
    spawned->waitfor(state);
  }
}

bool EntityBase::check()
{
  bool result = true;
  for(vector<QueueBase*>::iterator iter = queues.begin();
      iter != queues.end();
      iter++)
    {
      QueueBase* q = *iter;
      if(!q->empty()) {
        result = false;
        cerr << "Queue " << q->name() << " in entity " << name()
             << " still contains events." << endl;
      }
    }
  return result;
}

/****************************************************************************/

void System::add(EntityBase* entity) {
  entities.push_back(entity);
}

void System::terminate() {
  _terminate = true;
#ifndef NDEBUG
  abort();
#endif
}

bool System::check()
{
  bool result = true;
  for(vector<EntityBase*>::iterator iter = entities.begin();
      iter != entities.end();
      iter++)
    {
      if(!((*iter)->check()))
        result = false;
    }
  return result;
}

void System::start() {
  now = 0;
  while(run())
    ;
}

bool System::run(Time until) {
  _terminate = false;
  for(vector<EntityBase*>::iterator iter = entities.begin();
      iter != entities.end();
      iter++)
    {
      EntityBase* entity = *iter;
      entity->pushstate(0 NAMEDARG(entity->name() + "->" + "main"));
    }
  while(!_terminate) {
    StateBase* state;
    if((state = statesActive.head())) {
      assert(!state->isblocked());
      assert(!state->nextcondition && !state->condition);
      BUG(cerr << "System::cycle; stepping " << state->name() << endl);
      if(state->step()) {
        BUG(cerr << "System::cycle; finish state " << state->name() << endl);
        statesActive.remove(state);
        state->satisfy();
        delete state;
      }
    } else if((state = statesWaiting.pop())) {
      assert(state->time >= now);
      BUG(cerr << "System::cycle; timestep " << (state->time - now) << endl);
      now = state->time;
      state->_blocked = false;
      statesActive.append(state);
      if(until>=0 && state->time >= until)
        return true;
    } else {
      _terminate = true;
    }
  }
  return false;
}

std::ostream& operator<< (std::ostream& ost, const System& system) {
  ost << "System [" << &system << "] \"" << system.name() << "\""
      << " time=" << system.now << endl;
  ost << " -entities:" << endl;
  for(vector<EntityBase*>::const_iterator iter = system.entities.begin();
      iter != system.entities.end();
      iter++)
    {
      ost << **iter;
    }
  ost << " -states active:" << endl;
  ost << system.statesActive;
  ost << " -states waiting:" << endl;
  ost << system.statesWaiting;
  ost << " -states blocked:" << endl;
  ost << system.statesBlocked;
  return ost;
}

/****************************************************************************/

#ifdef HAVE_LIMITS_H
#include <limits.h>
#else
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#endif

List::List() {
  base = new StateBase((StateBase*)0, -1);
}

List::~List() {
  delete base;
}

std::ostream& operator<< (std::ostream& ost, const List& list) {
  for(StateBase* n=list.base->right; n && n!=list.base; n=n->right)
    n->output(ost);
  return ost;
}

RBTree::RBTree()
{
  // nil node has the minimum allowed time as key value.  Other nodes must
  // have a time value (key) of >= 0.
  nil = new StateBase((StateBase*)0, -1);
  root = new StateBase(nil, INT_MAX);
}

RBTree::~RBTree()
{
  delete nil;
  delete root;
}

StateBase*
RBTree::nextStateBase(StateBase* x) const
{
  StateBase* y;
  if(nil != ((y = x->right))) { /* assignment to y is intentional */
    while(y->left != nil) /* returns the minimum of the right subtree of x */
      y = y->left;
    return y;
  } else {
    y = x->parent;
    while(x == y->right) { /* sentinel used instead of checking for nil */
      x = y;
      y = y->parent;
    }
    if(y == root)
      return nil;
    return y;
  }
}

void
RBTree::removeFixUp(StateBase* x)
{
  StateBase* w;
  StateBase* rootLeft = root->left;
  while(!x->balance && rootLeft != x) {
    if(x == x->parent->left) {
      w = x->parent->right;
      if(w->balance) {
        w->balance = 0;
        x->parent->balance = 1;
        leftRotate(x->parent);
        w = x->parent->right;
      }
      if(!w->right->balance && !w->left->balance) { 
        w->balance = 1;
        x = x->parent;
      } else {
        if(!w->right->balance) {
          w->left->balance = 0;
          w->balance = 1;
          rightRotate(w);
          w = x->parent->right;
        }
        w->balance = x->parent->balance;
        x->parent->balance = 0;
        w->right->balance = 0;
        leftRotate(x->parent);
        x = rootLeft; /* this is to exit while loop */
      }
    } else { /* the code below is has left and right switched from above */
      w = x->parent->left;
      if(w->balance) {
        w->balance = 0;
        x->parent->balance = 1;
        rightRotate(x->parent);
        w = x->parent->left;
      }
      if(!w->right->balance && !w->left->balance) { 
        w->balance = 1;
        x = x->parent;
      } else {
        if(!w->left->balance) {
          w->right->balance = 0;
          w->balance = 1;
          leftRotate(w);
          w = x->parent->left;
        }
        w->balance = x->parent->balance;
        x->parent->balance = 0;
        w->left->balance = 0;
        rightRotate(x->parent);
        x = rootLeft; /* this is to exit while loop */
      }
    }
  }
  x->balance = 0;
}

StateBase*
RBTree::remove(StateBase* z)
{
  StateBase* y;
  StateBase* x;
  y = ((z->left == nil) || (z->right == nil)) ? z : nextStateBase(z);
  x = (y->left == nil) ? y->right : y->left;
  if (root == (x->parent = y->parent)) {
    /* assignment of y->p to x->p is intentional */
    root->left = x;
  } else {
    if(y == y->parent->left) {
      y->parent->left = x;
    } else {
      y->parent->right = x;
    }
  }
  if(y != z) { /* y should not be nil in this case */
    /* y is the node to splice out and x is its child */
    y->left = z->left;
    y->right = z->right;
    y->parent = z->parent;
    z->left->parent = z->right->parent = y;
    if(z == z->parent->left)
      z->parent->left = y;
    else
      z->parent->right = y;
    if(!y->balance) {
      y->balance = z->balance;
      removeFixUp(x);
    } else
      y->balance = z->balance;
  } else {
    if(!z->balance)
      removeFixUp(x);
  }
  return z;
}

StateBase*
RBTree::head()
{
  StateBase* run = root->left;
  if(!run || run == nil)
    return 0;
  while(run->left && run->left != nil)
    run = run->left;
  return run;
}

StateBase*
RBTree::pop()
{
  StateBase* run = head();
  return ( run ? remove(run) : 0 );
}

std::ostream& operator<< (std::ostream& ost, const RBTree& tree) {
  tree.output(ost, tree.root);
  return ost;
}

void RBTree::output(std::ostream& ost, StateBase *n) const {
  if(n->left && n->left!=nil)
    output(ost, n->left);
  if(n != root && n != nil)
    n->output(ost);
  if(n->right && n->right!=nil)
    output(ost, n->right);
}

/****************************************************************************/

}
