# Sample scheduler — checkpoint 12

`cts::SampleScheduler` reads rate limits from the `Device` registry copied at `define`. A later edit of the caller's `ChannelSpec` does not widen that stored min/max. Rates outside the stored interval, non-finite rates, and channels without a stored range are rejected and do not move the deadline.

`setRate` replaces only the next deadline (`now + period`). It does not emit a sample and does not change the drop counter.

`poll` returns at most one due channel. If several periods were missed, the extras increment the drop counter and are not emitted as a burst. The following deadline is placed after `now`. Time is microseconds. This does not read a sensor and does not publish.
