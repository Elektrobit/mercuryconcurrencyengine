//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
@file scheduler.hpp
This header is the heart of this library's code. Much of the code here is 
unusual and indirect, but necessary, to facilitate the functionality provided by 
the greater codebase.
*/
#ifndef __MERCURY_COROUTINE_ENGINE_SCHEDULER__
#define __MERCURY_COROUTINE_ENGINE_SCHEDULER__

// c 
#include <limits.h>

// c++
#include <memory>
#include <condition_variable>
#include <deque>
#include <thread>
#include <exception>
#include <utility>
#include <type_traits>

// local 
#include "function_utility.hpp"
#include "atomic.hpp"
#include "coroutine.hpp"

// test, only uncomment for development of this library
//#include "dev_print.hpp"

namespace mce {

/**
 @brief an interface for implementing lifecycle control operations
 */
struct lifecycle 
{
    /// an enumeration which represents the lifecycle object's current state
    enum state 
    {
        ready, /// is initial ready state
        running, /// is running
        suspended, /// temporarily halted by a call to suspend()
        halted /// permanently halted by a call to halt() 
    };

    /// virtual interface for implementors of lifecycle
    struct implementation {
        virtual ~implementation() { }
        virtual state get_state_impl() = 0;
        virtual bool suspend_impl() = 0;
        virtual void resume_impl() = 0;
        virtual void halt_impl() = 0;
        friend struct lifecycle;
    };

    /**
     Any implementation of lifecycle should pass its `this` pointer to its 
     lifecycle constructor.
     */
    lifecycle(implementation* self) : self_(self), root_(nullptr) { }

    /**
     Call public methods with the root lifecycle implementation's methods.

     Lifecycle objects can be structured as a tree, where lifecycle objects own 
     other lifecycle objects:
     root - child1 - child1-child1
                   - child1-child2
          - child2 - child2-child1
                   - child2-child2
          - etc

    If each child is given the root's pointer, then root's `_impl()` methods 
    will be called when the child's public methods are called. This allows 
    synchronized lifecycle operations no matter where in the tree `lifecycle` 
    public methods are called from.

    In short: if `suspend()`/`resume()`/`halt()`/etc are called on a child 
    object, and the child object was constructed with a root lifecycle pointer, 
    the root's `suspend_impl()`/`resume_impl()`/`halt_impl()` methods will 
    *actually* be called.

    All parent `lifecycle` `_impl()` methods should only call child 
    `_impl()` methods when interacting with child `lifecycle` objects. IE, 
    `_impl()` methods should *not* call public `lifecycle` methods.
     */
    lifecycle(implementation* self, implementation* root) :
        self_(self),
        root_(root)
    { }

    virtual ~lifecycle() { }

    /// return the state of the lifecycle
    inline state get_state()
    {
        if(root_){ return root_->get_state_impl(); }
        else { return self_->get_state_impl(); }
    }

    /** 
     @brief temporarily suspend operations

     @return `false` if lifecycle is halted, else `true`
     */
    inline bool suspend()
    {
        if(root_){ return root_->suspend_impl(); }
        else { return self_->suspend_impl(); }
    }

    /**
     @brief resume any current or future call to `run()` after an `suspend()`
     */
    inline void resume() 
    {
        if(root_){ root_->resume_impl(); }
        else { self_->resume_impl(); }
    }

    /**
     @brief halt and join lifecycle execution 
     
     As a general rule, `mce::lifecycle` implementations are intended to run 
     indefinitely and halt() should only be called on process shutdown. Failure 
     to do so can cause strange program errors where code which is expected to 
     run does not.
     */
    inline void halt()
    {
        if(root_){ root_->halt_impl(); }
        else { self_->halt_impl(); }
    }

private:
    implementation* self_;
    implementation* root_;
};

struct scheduler;

namespace detail {

// always points to the true scheduler running on the thread
scheduler*& tl_this_scheduler();

// points to the scheduler accessible by this_scheduler, which is generally 
// the same as tl_this_scheduler but may be different in certain cases
scheduler*& tl_this_scheduler_redirect();

template <typename T>
using queue = std::deque<T>;

}


//-----------------------------------------------------------------------------
// scheduler
//-----------------------------------------------------------------------------

/// returns true if calling scope is executing inside a running scheduler
bool in_scheduler();

/// returns a shared pointer to the scheduler the calling scope is running in
scheduler& this_scheduler();

/** 
 @brief object responsible for scheduling and executing coroutines
 
 mce::scheduler cannot be created directly, it must be created by calling 
 mce::scheduler::make() 
    
 Scheduler API, unless otherwise specified, is threadsafe and coroutine-safe.
 That is, it can be called from anywhere safely, including from within a 
 coroutine running on the scheduler which is being accessed.
*/
struct scheduler : public lifecycle, protected lifecycle::implementation
{
    /**
     @brief fundamental structure for allowing blocking of coroutines running on a scheduler

     Ths object is used by the `scheduler::parkable` object, which implements 
     high level blocking functionality needed by *both* coroutines and threads. 

     In comparison, this is a more basic type is only for blocking coroutines, 
     and requires more manual work because of its more generic nature. However, 
     this type is exposed in case it is required by user code.

     All higher level coroutine blocking mechanics are built on this structure.
     A mutable reference to a park::continuation is passed to 
     `scheduler::request_park()`, which will register the continuation with 
     the `scheduler` running on the current thread and `yield()` control to said 
     `scheduler`, suspending execution of the calling coroutine.

     When the `scheduler` resumes control, it will assign the just running 
     coroutine to the specified `std::unique_ptr<coroutine>`, and will assign its smart 
     pointer to the specified `std::shared_ptr<scheduler`. At this point, control of the 
     given coroutine completely leaves the `scheduler`, it is up to the 
     destination code to decide what to do with the coroutine.

     After passing the coroutine to its destination, the specified `cleanup()`
     method will be called with `memory`. This can be any operation, accepting 
     any data as an argument. A common usecase is to unlock an atomic lock 
     object.
     */
    struct park 
    {
        struct continuation
        {
            // pointer to coroutine storage location
            std::unique_ptr<mce::coroutine>* coroutine; 

            // memory to source scheduler storage location
            std::weak_ptr<mce::scheduler>* source;

            // arbitrary memory
            void* memory; 

            // cleanup procedure executed after acquiring coroutine
            void(*cleanup)(void*); 
        };

        /**
         @brief the fundamental operation required for blocking a coroutine running in a scheduler

         Direct usage of this is not recommended unless it is truly required, prefer
         scheduler::parkable instead.

         It is an error if this is called outside of a coroutine running in a 
         scheduler.
        */
        static inline void suspend(continuation& c)
        {
            // can safely write and read these variables without synchronization 
            // because they are only read/written by this thread, and this 
            // thread will only read/write to said values during a `run()` call
            detail::tl_this_scheduler()->park_continuation_ = &c;
            this_coroutine()->yield();
        }
    };

    /** 
     @brief object containing information to block and unblock a coroutine (running in a scheduler) or thread 

     This object abstracts the details of coroutine and thread blocking into a 
     unified high level park()/unpark() API.

     Parking is the operation that allows a given coroutine to suspend its own 
     execution in a scheduler until said coroutine is unparked, upon which it 
     will be rescheduled for execution. This functionality is typically used 
     only by basic coroutine blocking mechanisms (see chan, buffered_channel, 
     unbuffered_channel, united_mutex, united_condition_variable, etc.).

     A blocking operation is done by the calling code creating a parkable object
     and using said object to manipulate its execution state by calling 
     parkable::park() to suspend execution until *another* coroutine or thread 
     calls the parkable::unpark() function.

     Typical usage is:
     - create a parkable object on the stack (or some other privately managed memory) of some code which is about to block
     - share a pointer to the parkable to other code through some mechanism
     - call parkable::park()
     - some other code calls parkable::unpark() on the pointer when the code needs to be resumed
    */
    struct parkable
    {
        class unpark_exception : public std::exception 
        {
        public:
            virtual const char* what() const throw()
            {
                return "Cannot unpark a parkable that is not parked";
            }
        };

        parkable() { }
        ~parkable() { }

        /// blocking call until unpark, unlocks and relocks given lock as necessary 
        template <typename LOCK>
        void park(LOCK& lk)
        {
            if(in_scheduler())
            {
                wc = waiting_context::make<scheduled_context<LOCK>>();
            }
            else if(in_coroutine())
            {
                wc = waiting_context::make<coroutine_context<LOCK>>();
            }
            else
            {
                wc = waiting_context::make<thread_context<LOCK>>();
            }

            wc->wait((void*)&lk);
        }

        /**
         @brief unblock parked operation and reschedule it for execution.
         @return true if the unpark operation succeeded, else false
        */
        template <typename LOCK>
        void unpark(LOCK& lk)
        {
            if(wc) { wc->notify(); }
            else
            {
                lk.unlock();
                throw unpark_exception();
            }
        }

    private:
        struct waiting_context 
        {
            template <typename CONTEXT>
            static std::unique_ptr<waiting_context> make()
            {
                return std::unique_ptr<waiting_context>(
                    static_cast<waiting_context*>(new CONTEXT));
            }

            virtual ~waiting_context() { }
            virtual void wait(void* m) = 0;
            virtual void notify() = 0;
        };

        template <typename LOCK>
        struct scheduled_context : public waiting_context
        {
            virtual ~scheduled_context(){}

            inline void wait(void* m) 
            {
                LOCK& lk = *((LOCK*)m);

                // Indirectly retrieve this coroutine's pointer, with the
                // side effect of the calling context releasing all
                // ownership of said pointer.
                park::continuation pc{&co, &wsch, m, unlocker<LOCK>};

                //blocking call until unpark
                park::suspend(pc);

                // reacquire lock after unpark()
                lk.lock();
            }

            inline void notify()
            {
                auto sch = wsch.lock();

                if(sch) { sch->schedule(std::move(co)); }
            }

            std::unique_ptr<coroutine> co;
            std::weak_ptr<scheduler> wsch;
        };

        template <typename LOCK>
        struct coroutine_context : public waiting_context
        {
            virtual ~coroutine_context() { }

            inline void wait(void* m) 
            {
                LOCK& lk = *((LOCK*)m);

                co_not_ready = true;
                while(co_not_ready)
                {
                    lk.unlock();
                    this_coroutine()->yield();
                    lk.lock();
                }
            }

            inline void notify()
            { 
                // there is no coroutine to internally manage, break
                // out of yield loop
                co_not_ready = false;
            }

            bool co_not_ready = false;
        };

        template <typename LOCK>
        struct thread_context : public waiting_context
        {
            virtual ~thread_context() { }

            inline void wait(void* m)
            {
                LOCK& lk = *((LOCK*)m);

                do 
                {
                    cv.wait(lk);
                } while(!ready);

                ready = false;
            }

            inline void notify()
            {
                ready = true;
                cv.notify_one();
            }

            bool ready = false;
            std::condition_variable_any cv;
        };

        template <typename LOCK>
        static inline void unlocker(void* lk)
        {
            ((LOCK*)lk)->unlock();
        }

        std::unique_ptr<waiting_context> wc;
    };

    /// most straightforward blocked queue
    typedef mce::detail::queue<parkable*> parkable_queue;

    /**
    A generic interface to store a function to unblock a parkable. void* m is  
    implementation specific memory (IE, the implementation can pass whatever it 
    wants here). 

    Using this in a blocked queue allows abstracting operation specific behavior 
    to a callback. 

    This object is convertable to it's associated parkable*, allowing searches 
    for a specific operation if necessary.
    */
    struct parkable_notify : public std::function<void(void*)> 
    {
        template <typename... As>
        parkable_notify(parkable* p, As&&... as) : 
            std::function<void(void*)>(std::forward<As>(as)...),
            parkable_(p)
        { }

        // convert to parkable*
        inline operator parkable*() { return parkable_; }

    private:
        parkable* parkable_;
    };

    /// blocked queue for parkable_notify structs
    typedef mce::detail::queue<parkable_notify> parkable_notify_queue;

    virtual ~scheduler()
    {
        // ensure all coroutines are manually deleted
        clear_task_queue_();
    }

    /// return an allocated and initialized scheduler
    static inline std::shared_ptr<scheduler> make(lifecycle::implementation* root=nullptr)
    {
        scheduler* sp = new scheduler(root);
        std::shared_ptr<scheduler> s(sp);
        s->self_wptr_ = s;
        return s;
    };
    
    /**
     @brief run the scheduler, continuously executing coroutines

     This procedure can only be called by one caller at a time, and will block 
     the caller until `suspend()` or `halt()` is called. 

     Execution of coroutines by the caller of `run()` can be paused by calling 
     `suspend()`, causing `run()` to return `true`. Further calls to `run()` 
     will block until `resume()` is called.
     
     If `halt()` was previously called or `run()` is already evaluating 
     somewhere else will immediately return `false`.

     A simple usage of this feature is calling `run()` in a loop:
     ```
     while(my_scheduler->run()) 
     { 
         // do other things after suspend()
     }

     // do other things after halt()
     ```

     Doing so allows coroutine execution on this scheduler to be "put to sleep" 
     until `resume()` is called elsewhere. IE, this blocks the entire scheduler,
     and all coroutines scheduled on it, until `resume()` is called.

     This function is heavily optimized as it is a processing bottleneck.

     @return `true` if `run()` was suspended by `suspend()`, else `false`
     */
    inline bool run()
    {
        // stack variables 

        // the currently running coroutine
        coroutine* cur_co = nullptr;

        // only call function tl_this_scheduler() once to acquire reference to 
        // thread shared scheduler pointer 
        scheduler*& tl_cs = detail::tl_this_scheduler();
        scheduler*& tl_cs_re = detail::tl_this_scheduler_redirect();

        // acquire the parent, if any, of the current coroutine scheduler
        scheduler* parent_cs = tl_cs;

        // clarity flag to indicate this scheduler is running inside another 
        // scheduler on the same thread
        bool child = parent_cs;

        // both can be pushed without a lock because self_wptr_ is only written 
        // in threadsafe scheduler::make() 
        
        // temporarily reassign thread_local this_scheduler state to this scheduler
        auto push_scheduler_state_ = [&]
        { 
            tl_cs = this; 
            tl_cs_re = this;
        };

        // restore parent thread_local this_scheduler state
        auto pop_scheduler_state_ = [&]
        { 
            tl_cs = parent_cs; 
            tl_cs_re = parent_cs;
        };

        push_scheduler_state_();

        std::unique_lock<mce::spinlock> lk(lk_);

        // this should be trivially inlined by the compiler
        auto execute_co = [&]
        {
            // Acquire a new task. Don't bother swapping, it's slower than
            // copying the pointer
            cur_co = task_queue_.front();
                
            // unlock scheduler state when running a task
            lk.unlock();

            // execute coroutine
            cur_co->run();

            // if park_continuation_ is set that means the coroutine is 
            // requesting to be parked.
            if(park_continuation_)
            {
                auto& pc = *park_continuation_;
                *(pc.coroutine) = std::unique_ptr<coroutine>(cur_co);
                *(pc.source) = self_wptr_;
                pc.cleanup(pc.memory);
                park_continuation_ = nullptr;
                lk.lock();
                task_queue_.pop_front();
            }
            else if(cur_co->complete())
            { 
                // cleanup coroutine
                delete cur_co;
                lk.lock(); 
                task_queue_.pop_front();
                --scheduled_;
            }
            else 
            { 
                // re-enqueue coroutine
                lk.lock();
                task_queue_.pop_front();
                task_queue_.push_back(cur_co); 
            } 
        };

        // block until no longer suspended
        while(state_ == lifecycle::state::suspended) { resume_wait_(lk); }

        // if halt() has been called, return immediately
        // only one caller of run() is possible
        if(state_ == lifecycle::state::ready)
        {
            // the current caller of run() claims this scheduler, any calls to 
            // run() while it is already running will halt the scheduler 
            state_ = lifecycle::state::running; 
        
            try 
            {
                if(child)
                {
                    // this scheduler is running inside another scheduler
                    while(can_continue_())
                    {
                        if(task_queue_.size())
                        {
                            execute_co();

                            lk.unlock();

                            pop_scheduler_state_();

                            // Allow parent scheduler to run. This must be 
                            // called when scheduler lock is *not* held and 
                            // parent tl_this_scheduler() state is restored.
                            this_coroutine()->yield();
                            
                            push_scheduler_state_();

                            lk.lock();
                        }
                        else 
                        { 
                            pop_scheduler_state_();

                            while(task_queue_.empty() && can_continue_())
                            { 
                                tasks_available_child_wait_(lk);
                            }

                            push_scheduler_state_();
                        }
                    }
                }
                else 
                {
                    // root scheduler evaluation loop
                    while(can_continue_())
                    {
                        if(task_queue_.size()) { execute_co(); }
                        else 
                        { 
                            while(task_queue_.empty() && can_continue_())
                            { 
                                tasks_available_wait_(lk);
                            }
                        }
                    }
                }
            }
            catch(...) // catch all other exceptions 
            {
                // free memory we errored out of
                if(cur_co) { delete cur_co; }

                // reset state in case of uncaught exception
                if(tl_cs == this)
                {
                    tl_cs = parent_cs; // pop
                }

                std::rethrow_exception(std::current_exception());
            }
        }

        pop_scheduler_state_();

        if(state_ == lifecycle::state::suspended) 
        {
            // reset scheduler state so run() can be called again
            reset_flags_();
            return true;
        }
        else 
        { 
            // clear task queue so coroutine controlled memory can be released 
            clear_task_queue_();

            halt_complete_ = true;

            // notify any listeners that the scheduler is halted
            halt_complete_cv_.notify_all();

            while(halt_complete_waiters_.size())
            {
                halt_complete_waiters_.front()->unpark(lk_);
                halt_complete_waiters_.pop_front();
            }

            return false; 
        }
    }

    /**
     @brief schedule allocated coroutine(s)

     Arguments to this function can be:
     - an allocated `std::unique_ptr<mce::coroutine>`
     - an iterable container of allocated `std::unique_ptr<mce::coroutine>`s

     After the above argument types, any remaining arguments can be:
     - a Callable (followed by any arguments for the Callable) 

     The user can manually construct allocated 
     `std::unique_ptr<mce::coroutine>`s with `mce::coroutine::make()`

     Multi-argument schedule()s hold the scheduler's lock throughout (they are 
     simultaneously scheduled). This allows runtime order guarantees.

     @param a the first argument
     @param as any remaining arguments
     */
    template <typename A, typename... As>
    void schedule(A&& a, As&&... as)
    { 
        std::unique_lock<mce::spinlock> lk(lk_);

        if(state_ != lifecycle::state::halted)
        {
            schedule_(std::forward<A>(a), std::forward<As>(as)...);
        }
    }

    /**
     @brief a struct allowing comparison of scheduler workload

     This object represents a single value of a fundamental, word sized type. In 
     theory, it should be equally efficient to copy around as a fundamental 
     type. 

     It also provides introspection and utility methods.
     */
    struct measurement 
    {
        measurement() : weight_(0) { }
        measurement(const measurement& rhs) : weight_(rhs.weight_) { }
        measurement(measurement&& rhs) : weight_(rhs.weight_) { }

        /**
         Perform an operation which concatenates the enqueued count with the 
         scheduled count, such that the first half of the returned size_t 
         will be the enqueued count while the second half is the scheduled 
         count. 

         IE, enqueued task count has higher priority when determining load, but 
         if said count is equal, the tie can be broken by the scheduled task 
         count, which includes blocked coroutines.

         This mechanism allows for sanely measuring scheduling load in the 
         average case, while additionally protecting against edge cases without 
         adding much processing overhead and almost no extra lock contention.
         */
        measurement(size_t enqueued, size_t scheduled) : 
            weight_(
                (enqueued > right_mask ? right_mask : enqueued) << half_width |
                (scheduled > right_mask ? right_mask : scheduled))
        { }

        inline measurement& operator=(const measurement& rhs)
        {
            weight_ = rhs.weight_;
            return *this;
        }

        inline measurement& operator=(measurement&& rhs)
        {
            weight_ = rhs.weight_;
            return *this;
        }

        inline bool operator ==(const measurement& rhs)
        {
            return weight_ == rhs.weight_;
        }

        inline bool operator <(const measurement& rhs)
        {
            return weight_ < rhs.weight_;
        }
        
        inline bool operator <=(const measurement& rhs)
        {
            return weight_ <= rhs.weight_;
        }

        /// convert to a directly comparable fundamental type
        inline operator size_t() const { return weight_; }
        
        /// count of all scheduled coroutines, blocked or enqueued
        inline size_t scheduled() const { return weight_ & right_mask; }

        /// count of coroutines actively enqueued for execution
        inline size_t enqueued() const { return (weight_ & left_mask) >> half_width; }
        
        /// count of coroutines blocked on the scheduler
        inline size_t blocked() const { return scheduled() - enqueued(); }

    private:
        static constexpr size_t size_width = sizeof(size_t) * 8;
        static constexpr size_t half_width = size_width / 2;
        static constexpr size_t right_mask = SIZE_MAX >> half_width;
        static constexpr size_t left_mask = SIZE_MAX << half_width;

        size_t weight_; /// the value representing scheduler load 
    };

    /**
     The returned value is a best-effort attempt to represent the scheduler's 
     current scheduling load.

     The returned value is useful for finding the lowest scheduling load among 
     multiple schedulers, it can be directly compared using `<` operator.

     @return a value representing the current scheduling load
     */
    inline measurement measure()
    {
        lk_.lock();

        size_t enqueued = task_queue_.size();
        size_t scheduled = scheduled_;

        lk_.unlock();

        return { enqueued, scheduled };
    }

    /// return a copy of this scheduler's shared pointer by conversion
    inline operator std::shared_ptr<scheduler>() { return self_wptr_.lock(); }

protected:
    inline state get_state_impl() 
    {
        std::unique_lock<mce::spinlock> lk(lk_);
        return state_;
    }

    inline bool suspend_impl()
    {
        std::unique_lock<mce::spinlock> lk(lk_);

        if(state_ == lifecycle::state::halted) { return false; }
        else 
        {
            state_ = lifecycle::state::suspended;
            // wakeup scheduler if necessary from waiting for tasks to force 
            // run() to exit
            tasks_available_notify_();
            return true;
        }
    }

    inline void resume_impl()
    {
        std::unique_lock<mce::spinlock> lk(lk_);
        
        if(state_ == lifecycle::state::suspended) 
        { 
            state_ = lifecycle::state::ready; 
            resume_notify_();
        }
    }

    inline void halt_impl()
    { 
        std::unique_lock<mce::spinlock> lk(lk_);
        
        // set the scheduler to the permanent halted state
        state_ = lifecycle::state::halted;

        // wakeup from suspend if necessary
        resume_notify_();

        // handle case where halt is called from within running scheduler
        if(in_scheduler() && &(this_scheduler()) == this)
        {
            // no need to notify here, running on the thread we are notifying
            lk.unlock();
            this_coroutine()->yield();
        }
        else 
        {
            // wakeup scheduler if necessary
            tasks_available_notify_();

            // handle case where halt() is called by a coroutine running in
            // a different scheduler
            if(in_coroutine())
            {
                while(!halt_complete_)
                {
                    parkable p;
                    halt_complete_waiters_.push_back(&p);
                    p.park(lk);
                }
            }
            // handle final case where halt() is called from another std::thread
            else
            {
                while(!halt_complete_){ halt_complete_cv_.wait(lk); }
            }
        }
    }
 
private:
    scheduler(lifecycle::implementation* root) : 
        lifecycle(this,root),
        state_(lifecycle::state::ready), // state_ persists between suspends
        scheduled_(0) // scheduled_ persists between suspends
    { 
        reset_flags_(); // initialize flags
    }

    // Reset scheduler state flags, etc. Does not reset scheduled coroutine
    // queues. 
    //
    // This method can ONLY be safely called by the constructor or run()
    inline void reset_flags_() 
    {
        halt_complete_ = false;
        waiting_for_resume_ = false;
        waiting_for_tasks_ = false;
        tasks_available_park_ = nullptr;
    }

    // abstract frequently used inlined state to this function for correctness
    inline bool can_continue_() { return state_ < lifecycle::state::suspended; }

    void clear_task_queue_()
    {
        while(task_queue_.size()) 
        {
            delete task_queue_.front();
            task_queue_.pop_front();
        }
    }

    // attempt to wait for resumption
    template <typename LOCK>
    void resume_wait_(LOCK& lk)
    {
        waiting_for_resume_ = true;
        resume_cv_.wait(lk);
    }
    
    // notify caller of run() that resume() has been called 
    inline void resume_notify_()
    {
        // only do notify if necessary
        if(waiting_for_resume_) 
        {
            waiting_for_resume_ = false;
            resume_cv_.notify_one();
        }
    }

    inline void update_running_state_()
    {
        // reacquire running state in edgecase where suspend() and resume()
        // happened quickly
        if(state_ == lifecycle::state::ready) 
        {
            state_ = lifecycle::state::running;
        }
    }
  
    // attempt to wait for more tasks
    template <typename LOCK>
    void tasks_available_wait_(LOCK& lk)
    {
        waiting_for_tasks_ = true;
        tasks_available_cv_.wait(lk);
        update_running_state_();
    }
   
    template <typename LOCK>
    void tasks_available_child_wait_(LOCK& lk)
    {
        waiting_for_tasks_ = true;
        parkable p;
        tasks_available_park_ = &p;
        p.park(lk);
        update_running_state_();
    }

    // notify caller of run() that tasks are available
    inline void tasks_available_notify_()
    {
        // only do notify if necessary
        if(waiting_for_tasks_) 
        { 
            waiting_for_tasks_ = false; 

            if(tasks_available_park_)
            { 
                tasks_available_park_->unpark(lk_);
                tasks_available_park_ = nullptr;
            }
            else
            {
                tasks_available_cv_.notify_one(); 
            }
        }
    }

    // when all coroutines are scheduled, notify
    inline void schedule_() { tasks_available_notify_(); }
    
    template <typename... As>
    void schedule_(std::unique_ptr<coroutine>&& c, As&&... as)
    {
        // detect if A is a container or a Callable
        schedule_coroutine_(std::move(c));
        schedule_(std::forward<As>(as)...);
    }

    template <typename A, typename... As>
    void schedule_(A&& a, As&&... as)
    {
        // detect if A is a container or a Callable
        schedule_fallback_(
                detail::is_container<typename std::decay<A>::type>(),
                std::forward<A>(a), 
                std::forward<As>(as)...);
    }
    
    inline void schedule_coroutine_(std::unique_ptr<coroutine>&& c)
    {
        if(c)
        {
            ++scheduled_;
            task_queue_.push_back(c.release());
        }
    }

    template <typename Container, typename... As>
    void schedule_fallback_(std::true_type, Container&& coroutines, As&&... as)
    {
        for(auto& c : coroutines)
        {
            schedule_coroutine_(std::move(c));
        }

        schedule_(std::forward<As>(as)...);
    }

    template <typename Callable, typename... As>
    void schedule_fallback_(std::false_type, Callable&& cb, As&&... as)
    {
        schedule_coroutine_(
            coroutine::make(
                std::forward<Callable>(cb), 
                std::forward<As>(as)...));

        schedule_();
    }

    // the current state of the scheduler
    lifecycle::state state_; 
    
    // true if halt has completed, used for synchronizing calls to halt()
    bool halt_complete_; 
                  
    // a count of coroutines on this scheduler, including coroutines
    // actively queued to be run AND parked coroutines associated with this 
    // scheduler
    size_t scheduled_; 
    
    std::condition_variable_any resume_cv_;
    bool waiting_for_resume_;

    std::condition_variable_any tasks_available_cv_;
    bool waiting_for_tasks_; // true if waiting on tasks_available_cv_
    scheduler::parkable* tasks_available_park_;

    std::condition_variable_any halt_complete_cv_;
    scheduler::parkable_queue halt_complete_waiters_;   

    std::weak_ptr<scheduler> self_wptr_;

    // the continuation requested by the running coroutine
    park::continuation* park_continuation_ = nullptr;

    // queue holding scheduled coroutines. Raw coroutine pointers are used 
    // internally because we don't want even the cost of rvalue swapping 
    // unique pointers during internal operations (potentially 3 instructions), 
    // when we can deep copy a word size value intead. This requires careful 
    // calls to `delete` when a coroutine goes out of scope in this object. 
    std::deque<coroutine*> task_queue_;

    mce::spinlock lk_;
};

/// return `true` if calling code is running in a scheduler, else `false`
inline bool in_scheduler()
{
    return detail::tl_this_scheduler_redirect();
}

/**
 @brief acquire a reference to the scheduler running on the current thread

 This operation is guaranteed to not fail, but it will return an empty pointer 
 if called from a thread that is not running a scheduler.
 */
inline scheduler& this_scheduler()
{ 
    return *(detail::tl_this_scheduler_redirect()); 
}

}

#endif 
