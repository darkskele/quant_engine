#include "events/event_queue.hpp"

namespace engine::events
{
    void event_queue::push(event event)
    {
        // Push to queue
        queue_.push(std::move(event));
    }

    event event_queue::pop()
    {
        // Move from front
        auto event = std::move(queue_.front());
        // Pop and return
        queue_.pop();
        return event; // NRVO
    }

    bool event_queue::empty() const
    {
        return queue_.empty();
    }

} // namespace engine::events