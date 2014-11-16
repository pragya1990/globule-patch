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
#ifndef _FORNAX_H
#define _FORNAX_H
/* Arno's method of turning on/off debugging code currently clashes with
 * mine.
 */
#undef DEBUG
#undef NAMEDSTATES

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <list>
#include <vector>
#include <typeinfo>
#include <assert.h>
#include <cstdlib>
#ifdef WITH_DMALLOC
#define DMALLOC
#include <dmalloc.h>
#endif

#ifdef DEBUG
#ifndef BUG
#define BUG(ARG) ARG
#endif
#define NAMEDSTATES
#define NAMEDARG(ARG) ,ARG
#else
#ifndef BUG
#define BUG(ARG)
#endif
#ifdef NAMEDSTATES
#define NAMEDARG(ARG) ,ARG
#else
#define NAMEDARG(ARG)
#endif
#endif

namespace fornax {

using std::string;
using std::clog;
using std::endl;

class Blocker;
template <typename E> class Event;
class QueueBase;
template <typename E> class Queue;
class StateBase;
template <typename V> class State;
template <typename E> class EventState;
class EntityBase;
template <typename V> class EntityCore;
template <typename V, typename P> class Entity;
class System;
class List;
class RBTree;

template <typename V, typename P>
std::ostream& operator<< (std::ostream&, const Entity<V,P>&);
template <typename V>
std::ostream& operator<< (std::ostream&, const State<V>&);
template <typename E>
std::ostream& operator<< (std::ostream&, const Queue<E>&);
template <typename E>
std::ostream& operator<< (std::ostream&, const Event<E>&);
template <typename E>
std::ostream& operator<< (std::ostream&, const EventState<E>&);

/****************************************************************************/

typedef int StateId;
typedef long long Time;

class Blocker
{
  friend class StateBase;
protected:
  StateBase* listener;
public:
  Blocker() : listener(0) { };
  inline void satisfy();
  inline virtual void waitfor(StateBase* who);
};

template <typename E>
class Event : public Blocker {
  friend std::ostream& operator<< <> (std::ostream& ost, const Event<E>&);
private:
  E params;
  bool deleteOnReply;
public:
  Event(E p, bool selfDispose = false);
  virtual ~Event() {};
#ifdef NAMEDSTATES
  virtual string name() const { return "<unknown event>"; };
#endif
  E& args() {
    return params;
  };
  // inlining might work to allow prematurely delete the object
  void reply() {
    BUG(clog << "Event::reply; [" << this << "/" << name() << "] delete="
             << deleteOnReply << endl);
    satisfy();
    if(deleteOnReply)
      delete this; // delete before exiting the object code is allowed?
  };
  inline void selfDispose() {
    deleteOnReply = true;
  };
};

class QueueBase : public Blocker {
  friend class EntityBase;
  friend std::ostream& operator<< (std::ostream&, const QueueBase&);
protected:
  string _name;
  StateId stateid;
  EntityBase *owner;
  virtual std::ostream& output(std::ostream&) const;
public:
  //QueueBase(string name, int subroutine);
  QueueBase(EntityBase* entity, string name, int subroutine);
  virtual ~QueueBase() {};
  virtual string name() const { return _name; };
  virtual bool empty() { abort(); return true; };
};

template <typename E>
class Queue : public QueueBase {
  friend class EntityBase;
  template <typename V, typename P> friend class Entity;
  friend std::ostream& operator<< <> (std::ostream&, const Queue<E>&);
private:
  Event<E>* current;
  std::list<Event<E>*> events;
protected:
  virtual std::ostream& output(std::ostream& ost) const {
    return ost << *this;
  };
public:
  //Queue() : QueueBase("<unknown>"), current(0) { };
  //Queue(string name, int subroutine) : QueueBase(name,subroutine), current(0) { };
  Queue(EntityBase* entity, string name, int subroutine) : QueueBase(entity,name,subroutine), current(0) { };
  virtual ~Queue() { };
  virtual void waitfor(StateBase* who);
  void recv(StateBase* state);
  void send(Event<E> evt);
  void send(Event<E>* evt);
  void send(StateBase* state, Event<E>* evt);
  Event<E>* deliver(Time t, E& evtdata, bool keep=true);
  Event<E>* event() {
    return current;
  };
  virtual bool empty() { return events.empty(); };
};

class StateBase : public Blocker {
  friend class EntityBase;
  friend class Blocker;
  friend class List;
  template <typename E> friend class Queue;
  friend class RBTree;
  friend class System;
  friend std::ostream& operator<< (std::ostream& ost, const List&);
  friend std::ostream& operator<< (std::ostream& ost, const RBTree&);
private:
  bool _blocked;
  System* system;
  Blocker* condition; // back pointer from Blocker->listener
  StateBase* left;   // Node structure; points to left or previous
  StateBase* right;  // Node structure; points to right or next
  StateBase* parent; // Node structure; points to parent or 0
  int balance;  // Node structure; 0=black, 1=red
protected:
  StateBase* firstcondition;
  StateBase* nextcondition;
  Time time;    // Node structure; the key to use for sorting
private:
  // Used for Node structure solely
  StateBase(StateBase* ref, Time key)
    : _blocked(true), system(0), condition(0),
      left(ref?ref:this), right(ref?ref:this), parent(ref?ref:this),
      balance(0), firstcondition(0), nextcondition(0), time(key)
    { };
public:
  StateBase(System* system);
  StateBase(System* system, Time t);
  virtual ~StateBase() {};
private:
  virtual std::ostream& output(std::ostream&) const;
public:
#ifdef NAMEDSTATES
  virtual string name() const;
#endif
  virtual void terminate() { abort(); };
  virtual StateBase* pushstate(int NAMEDARG(string)) { abort(); return 0; }
  StateId error();
  virtual void waitfor(StateBase* who);
  void cancel(StateBase* who);
  inline void block();
  inline void block(Time until);
  inline void unblock();
  inline void cancel();
  inline bool isblocked() { return _blocked; };
  virtual bool step() { return true; }; // returns whether state finished
};

template <typename V>
class State : public StateBase {
  friend std::ostream& operator<< <> (std::ostream&, const State<V>&);
private:
  EntityCore<V>* owner;
#ifdef NAMEDSTATES
  string _name;
#endif
public:
  StateId id;
  State(EntityCore<V>* entity, StateId stateid NAMEDARG(string desc))
    : StateBase(&entity->system), owner(entity), id(stateid)
  {
#ifdef NAMEDSTATES
    _name = desc;
#endif
  }
  virtual ~State() {};
  virtual StateBase* pushstate(int stateid NAMEDARG(const string desc)) {
    State<V>* state = new State<V>(owner, stateid NAMEDARG(name()+"."+desc));
    state->vars = vars;
    return state;
  }
#ifdef NAMEDSTATES
  virtual string name() const { return _name; };
#endif
public:
  V vars;
  void wait(Time delay) {
    if(delay > 0) {
      block(owner->time() + delay);
      assert(!firstcondition);
      assert(!nextcondition);
    }
  };
  virtual void terminate();
  virtual inline bool step() {
    id = owner->step(this);
    return (id == 0);
  };
};

template <typename E>
class EventState : public StateBase, public Event<E> {
  friend std::ostream& operator<< <> (std::ostream&, const EventState&);
#ifdef NAMEDSTATES
  string _name;
#endif
  Queue<E>* queue;
public:
  EventState(System &system NAMEDARG(string desc), Queue<E>* q, Time t, E &p,
             bool keep=true)
    : StateBase(&system, t), Event<E>(p,!keep) NAMEDARG(_name(desc)), queue(q)
  { }
  virtual ~EventState() {};
  virtual StateBase* pushstate(int stateid NAMEDARG(const string desc)) {
    abort();
  }
#ifdef NAMEDSTATES
  virtual string name() const { return _name; };
#endif
public:
  virtual inline bool step() {
    queue->send(this);
    cancel();
    return false;
  };
};

/****************************************************************************/

class List
{
  friend std::ostream& operator<< (std::ostream& ost, const List&);
private:
  StateBase* base;
  /* For this class, the base node is a non-member node permanently present.
   * The right link of this node is the first node in the list while the left
   * is the last node in the list.  Notice the intuitive reversed
   * interpretation of left and right.
   */
public:
  List();
  ~List();
  inline void append(StateBase* n);
  inline StateBase* remove(StateBase* n);
  inline StateBase* head();
  inline StateBase* pop();
};

class RBTree
{
  friend std::ostream& operator<< (std::ostream& ost, const RBTree&);
private:
  StateBase* root;
  StateBase* nil;
public:
  RBTree();
  ~RBTree();
private:
  void output(std::ostream& ost, StateBase *n) const;
  inline void leftRotate(StateBase* x);
  inline void rightRotate(StateBase* y);
  inline void insertHelp(StateBase* z);
  StateBase* nextStateBase(StateBase* x) const;
  void removeFixUp(StateBase* x);
public:
  inline void insert(StateBase* x);
  StateBase* remove(StateBase* z);
  StateBase* head();
  StateBase* pop();
};

class System {
  friend class EntityBase;
  friend class StateBase;
  friend std::ostream& operator<< (std::ostream&, const System&);
private:
  bool _terminate;
  std::vector<EntityBase*> entities;
  Time now;
  string _name;
  List statesActive;
  RBTree statesWaiting;
  List statesBlocked;
  void add(EntityBase* entity);
public:
  System() : _terminate(false), now(0), _name("<unknown>") {};
  System(string name) : _name(name) {};
  virtual ~System() {};
  virtual string name() const { return _name; };
  void start(); // deprecated, use run() instead and ignore the bool
  bool run(Time _time = -1);
  void terminate();
  bool check();
  inline Time time() { return now; };
};

class EntityBase {
  friend class System;
  friend class QueueBase;
  template <typename E> friend class Queue;
  friend std::ostream& operator<< (std::ostream&, const EntityBase&);
private:
  string _name;
protected:
  System& system;
  std::vector<QueueBase*> queues;
  virtual std::ostream& output(std::ostream&) const;
public:
  EntityBase(System& environment);
  EntityBase(System& environment, string name);
  virtual ~EntityBase() {};
  virtual string name() const { return _name; };
  virtual void pushstate(int NAMEDARG(string)) { abort(); };
  inline Time time() { return system.time(); };
  void recv(StateBase* state, QueueBase& q);
  void recv(StateBase* state, int nqs, QueueBase** qs);
  void terminate();
  bool check();
};

template <typename V>
class EntityCore : public EntityBase {
  friend class State<V>;
protected:
  virtual std::ostream& output(std::ostream& ost) const {
    return ost << *this;
  };
public:
  EntityCore(System& environment)
    : EntityBase(environment) {};
  EntityCore(System& environment, string name)
    : EntityBase(environment, name) {};
  virtual ~EntityCore() {};
  virtual void pushstate(int NAMEDARG(string desc));
  virtual StateId step(State<V>*) { return 0; };
};

template <typename V, typename P>
class Entity : public EntityCore<V> {
  friend std::ostream& operator<< <> (std::ostream&, const Entity<V,P>&);
private:
  Queue<P> queue_main;
protected:
  virtual std::ostream& output(std::ostream& ost) const {
    return ost << *this;
  };
public:
  Entity(System& environment)
    : EntityCore<V>(environment), queue_main(this, "main", 0) {};
  Entity(System& environment, string name)
    : EntityCore<V>(environment, name), queue_main(this, "main", 0) {};
  virtual ~Entity() { delete queue_main.current; };
  virtual StateId step(State<V>*) { return 0; };
  void main(Event<P> p) {
    queue_main.send(p);
    queue_main.waitfor(0);
  }
  P& args() {
    return queue_main.event()->args();
  }
};

/****************************************************************************/

template <typename V> 
void State<V>::terminate() {
  owner->terminate();
}

#ifdef NOTDEFINED
template <typename V>
bool State<V>::step() {
  id = owner->step(this);
  return (id == 0);
}
#endif

/****************************************************************************/

template <typename E>
Event<E>::Event(E p, bool deleteflag)
  : Blocker()
{
  params = p;
  deleteOnReply = deleteflag;
}

/****************************************************************************/

template <typename E>
void Queue<E>::waitfor(StateBase* who) {
  if(events.empty()) {
    assert(!listener);
    BUG(clog << "Queue::waitfor; [" << this << "/" << name() << "] queue empty, blocking state " << who << endl);
    listener = who;
    who->block();
    assert(!listener->firstcondition);
    assert(!listener->nextcondition);
    listener->condition = this;
  } else {
    BUG(clog << "Queue::waitfor; [" << this << "/" << name() << "] event present, continuing state" << endl);
    current = *(events.begin());
    events.pop_front();
  }
}

template <typename E>
void Queue<E>::recv(StateBase* state) {
  StateBase* spawn = state->pushstate(stateid NAMEDARG(name()));
  BUG(clog << "Queue::recv; [" << this << "/" << name() << "]spawned state waitfor event in queue" << endl);
  waitfor(spawn);
  BUG(clog << "Queue::recv; original state waitfor spawned state" << endl);
  spawn->waitfor(state);
  BUG(clog << "Queue::recv; exit from subroutine " << state->blocked << endl);
}

template <typename E>
void Queue<E>::send(Event<E> evtCopy) {
  BUG(clog << "Queue::send#1; [" << this << "/" << name() << "] listener=" << listener << endl);
  Event<E>* evt = new Event<E>(evtCopy.args(), true);
  if(listener) {
    BUG(clog << "Queue::send; entity was already listening for event" << endl);
    satisfy();
    current = evt;
  } else {
    BUG(clog << "Queue::send; event will be queued" << endl);
    events.push_back(evt);
  }
}

template <typename E>
void Queue<E>::send(Event<E>* evt) {
  BUG(clog << "Queue::send#1; [" << this << "/" << name() << "] listener=" << listener << endl);
  if(listener) {
    BUG(clog << "Queue::send; entity was already listening for event" << endl);
    satisfy();
    current = evt;
  } else {
    BUG(clog << "Queue::send; event will be queued" << endl);
    events.push_back(evt);
  }
}

template <typename E>
void Queue<E>::send(StateBase* state, Event<E>* evt) {
  BUG(clog << "Queue::send#2; [" << this << "/" << name() << "] listener=" << listener << endl);
  if(listener) {
    BUG(clog << "Queue::send; entity was already listening for event" << endl);
    satisfy();
    current = evt;
  } else {
    BUG(clog << "Queue::send; event will be queued" << endl);
    events.push_back(evt);
  }
  evt->waitfor(state);
}

template <typename E>
Event<E> *Queue<E>::deliver(Time t, E& evtdata, bool keep) {
  return new fornax::EventState<E>(owner->system NAMEDARG("external"), this, t, evtdata, keep);
}

/****************************************************************************/

template <typename V>
void EntityCore<V>::pushstate(StateId stateid NAMEDARG(string desc))
{
  new State<V>(this, stateid NAMEDARG(desc));
  // is added automatically to statesActive
}

/****************************************************************************/

#include <iostream>

template <typename E>
std::ostream& operator<< (std::ostream& ost, const Event<E>& event) {
  ost << "       Event [" << &event << "] autodelete=" << event.deleteOnReply;
  ost << " listener=" << event.listener << std::endl;
  return ost;
}

template <typename E>
std::ostream& operator<< (std::ostream& ost, const Queue<E>& queue) {
  ost << "     Queue [" << &queue << "] \"" << queue.name() << "\""
      << " listener=" << queue.listener << std::endl;
  if(queue.current)
    ost << "      -current:" << std::endl << *queue.current;
  ost << "      -events:" << std::endl;
  for(typename std::list< Event<E>* >::const_iterator iter=queue.events.begin();
      iter != queue.events.end();
      iter++)
    {
      ost << **iter;
    }
  return ost;
}

template <typename V>
std::ostream& operator<< (std::ostream& ost, const State<V>& state) {
  ost << "    State [" << &state << "] pos=" << state.id;
  if(state.isblocked())
    ost << " blocked time=" << state.time;
  ost << " listener=" << state.listener << std::endl;
  return ost;
}

template <typename V, typename P>
std::ostream& operator<< (std::ostream& ost, const Entity<V,P>& entity) {
  ost << "  Entity [" << &entity << "] \"" << entity.name()
      << "\" system=" << &entity.system << std::endl
      << "   -queues:" << std::endl;
  for(typename std::vector< QueueBase* >::const_iterator iter=entity.queues.begin();
      iter != entity.queues.end();
      iter++)
    {
      ost << **iter;
    }
  ost << "   -states:" << std::endl;
  ost << "   -main queue:" << std::endl;
  ost << entity.queue_main;
  return ost;
}

/****************************************************************************/

inline void Blocker::waitfor(StateBase* who)
{
  assert(!listener);
  assert(!who->isblocked());
  listener = who;
  listener->block();
  who->condition = this;
}

inline void Blocker::satisfy()
{
  BUG(clog << "Blocker::satisfy; [" << this << "] listener=" << listener
           << " listener->listener=" << (listener?listener->listener:0)
           << endl);
  if(listener) {
    assert(listener->firstcondition == this || !listener->firstcondition);
    // listener->firstcondition = 0;
    if(listener->listener)
      listener->listener->cancel(listener);
    listener->unblock();
  }
}

inline void StateBase::waitfor(StateBase* who)
{
  BUG(clog << "Blocker::waitfor; [" << this << "/" << name() << "] "
           << "listener=" << who << "/" << who->name() << endl);
  listener = who;
  listener->block();
  assert(!nextcondition);
  nextcondition = listener->firstcondition;
  listener->firstcondition = this;
  listener->condition = this;
}

inline void StateBase::block()
{
  BUG(clog << "StateBase::block#1; [" << this << "/" << name() << "]" << endl);
  assert(!_blocked || time == -1);
  if(!_blocked) {
    system->statesActive.remove(this);
    _blocked = true;
    time = -1;
    system->statesBlocked.append(this);
  }
}

inline void StateBase::block(Time until)
{
  BUG(clog << "StateBase::block#2; [" << this << "/" << name() << "] "
           << "until=" << until << endl);
  assert(!_blocked);
  system->statesActive.remove(this);
  _blocked = true;
  time = until;
  system->statesWaiting.insert(this);
}

inline void StateBase::cancel()
{
  BUG(clog << "StateBase::cancel; [" << this << "/" << name() << "] "
           << "time=" << time << endl);
  if(_blocked) {
    if(time < 0) {
      assert(condition->listener == this);
      condition->listener = 0;
      system->statesBlocked.remove(this);
    } else
      system->statesWaiting.remove(this);
  } else
    system->statesActive.remove(this);
}

inline void StateBase::unblock()
{
  assert(_blocked);
  _blocked = false;
  if(time < 0) {
    assert(condition->listener == this);
    condition->listener = 0;
    condition = 0;
    firstcondition = 0;
    system->statesBlocked.remove(this);
  } else
    system->statesWaiting.remove(this);
  system->statesActive.append(this);
  assert(!condition && !nextcondition);
}

inline void List::append(StateBase* n)
{
  n->parent = 0;
  n->left = base->left;
  n->right = base;
  base->left->right = n;
  base->left = n;
}
inline StateBase* List::remove(StateBase* n) {
  n->left->right = n->right;
  n->right->left = n->left;
  n->left = n->right = 0;
  return n;
}
inline StateBase* List::head() {
  return (base->right != base ? base->right : 0);
}
inline StateBase* List::pop() {
  return remove(head());
}

inline void RBTree::leftRotate(StateBase* x)
{
  StateBase* y;
  y = x->right;
  x->right = y->left;
  if(y->left != nil)
    y->left->parent = x;
  y->parent = x->parent;   
  /* instead of checking if x->parent is the root as in the book, we */
  /* count on the root sentinel to implicitly take care of this case */
  if(x == x->parent->left)
    x->parent->left = y;
  else
    x->parent->right = y;
  y->left = x;
  x->parent = y;
}

inline void RBTree::rightRotate(StateBase* y)
{
  StateBase* x;
  x = y->left;
  y->left = x->right;
  if(nil != x->right)
    x->right->parent = y;
  x->parent = y->parent;
  if(y == y->parent->left)
    y->parent->left = x;
  else
    y->parent->right = x;
  x->right = y;
  y->parent = x;
}

inline void
RBTree::insertHelp(StateBase* z)
{
  StateBase* x;
  StateBase* y;
  z->left = z->right = nil;
  y = root;
  x = root->left;
  while(x != nil) {
    y = x;
    if(x->time > z->time)
      x = x->left;
    else
      x = x->right;
  }
  z->parent = y;
  if(y == root || y->time > z->time)
    y->left = z;
  else
    y->right = z;
}

inline void
RBTree::insert(StateBase* x)
{
  StateBase* y;
  insertHelp(x);
  x->balance = 1;
  while(x->parent->balance) { /* use sentinel instead of checking for root */
    if (x->parent == x->parent->parent->left) {
      y = x->parent->parent->right;
      if(y->balance) {
        x->parent->balance = 0;
        y->balance = 0;
        x->parent->parent->balance = 1;
        x = x->parent->parent;
      } else {
        if (x == x->parent->right) {
          x = x->parent;
          leftRotate(x);
        }
        x->parent->balance = 0;
        x->parent->parent->balance = 1;
        rightRotate(x->parent->parent);
      } 
    } else {
      y = x->parent->parent->left;
      if(y->balance) {
        x->parent->balance = 0;
        y->balance = 0;
        x->parent->parent->balance = 1;
        x = x->parent->parent;
      } else {
        if(x == x->parent->left) {
          x = x->parent;
          rightRotate(x);
        }
        x->parent->balance = 0;
        x->parent->parent->balance = 1;
        leftRotate(x->parent->parent);
      } 
    }
  }
  root->left->balance = 0;
}

/****************************************************************************/

}

#endif /* _FORNAX_H */
