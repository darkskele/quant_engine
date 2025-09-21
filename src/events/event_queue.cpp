#include "events/event_queue.hpp"

#include <stdexcept>

namespace engine::events
{
    void event_queue::push(event ev)
    {
        // Push to queue
        queue_.push(std::move(ev));
    }

    event event_queue::pop()
    {
        if (empty())
        {
            throw std::runtime_error("Queue empty!");
        }

        // Move from front
        auto ev = std::move(queue_.front());
        // Pop and return
        queue_.pop();
        return ev; // NRVO
    }

    bool event_queue::empty() const
    {
        return queue_.empty();
    }

    size_t event_queue::size() const noexcept
    {
        return queue_.size();
    }

} // namespace engine::events