package cp2022.solution;

import java.util.ArrayDeque;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Queue;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentSkipListSet;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicInteger;

import cp2022.base.Workplace;
import cp2022.base.WorkplaceId;
import cp2022.base.Workshop;


public class ConcurrentWorkshop implements Workshop {

    private final Map<WorkplaceId, Workplace> workplaces = new TreeMap<>();
    private final Map<WorkplaceId, WorkplaceId> nextWorkplace = new TreeMap<>();
    private final Map<WorkplaceId, Queue<WorkplaceId>> workplaceQueue = new TreeMap<>();
    private final Map<WorkplaceId, Semaphore> move = new TreeMap<>();
    private final Map<WorkplaceId, Semaphore> enter = new TreeMap<>();
    private final Map<WorkplaceId, Semaphore> work = new TreeMap<>();
    private final Map<Long, WorkplaceId> currentWorkplace = new ConcurrentHashMap<>();
    private final Map<Long, WorkplaceId> previousWorkplace = new ConcurrentHashMap<>();
    private final Map<Long, Integer> actionFinished = new HashMap<>();
    private final Queue<Long> actions = new ArrayDeque<>();
    private final Set<WorkplaceId> cycle = new ConcurrentSkipListSet<>();
    private final AtomicInteger enterCount = new AtomicInteger(0);
    private final Integer enterLimit;
    private final Queue<Semaphore> allowEnter = new ArrayDeque<>();
    private final Semaphore switchMutex = new Semaphore(1);

    public ConcurrentWorkshop(Collection<Workplace> workplaces) {
        this.enterLimit = 2 * workplaces.size() - 1;
        for (Workplace w : workplaces) {
            this.workplaces.put(w.getId(), new ConcurrentWorkplace(w, this));
            this.workplaceQueue.put(w.getId(), new ArrayDeque<>());
            this.move.put(w.getId(), new Semaphore(0));
            this.enter.put(w.getId(), new Semaphore(1, true));
            this.work.put(w.getId(), new Semaphore(1));
        }
    }

    @Override
    public Workplace enter(WorkplaceId wid) {
        try {
            Semaphore wait = new Semaphore(0);
            synchronized (actions) {
                if (!actions.isEmpty()) {
                    enterCount.incrementAndGet();
                }
                actions.add(-Thread.currentThread().getId());
                if (enterCount.get() > enterLimit) {
                    allowEnter.add(wait);
                } else {
                    wait.release();
                }
            }
            wait.acquire();
            enter.get(wid).acquire();
        } catch (InterruptedException e) {
            throw new RuntimeException("panic: unexpected thread interruption");
        }

        return goToWorkplace(wid);
    }

    @Override
    public Workplace switchTo(WorkplaceId wid) {
        Long threadId = Thread.currentThread().getId();
        synchronized (actions) {
            actions.add(threadId);
        }

        WorkplaceId currentWid = currentWorkplace.get(threadId);
        try {
            switchMutex.acquire();
        } catch (InterruptedException e) {
            throw new RuntimeException("panic: unexpected thread interruption");
        }

        if (detectCycle(wid, currentWid)) {
            WorkplaceId current = wid;
            while (current.compareTo(currentWid) != 0) {
                cycle.add(current);
                WorkplaceId next = nextWorkplace.get(current);
                move.get(current).release();
                current = next;
            }
            switchMutex.release();
        } else {
            if (!enter.get(wid).tryAcquire(1)) {
                nextWorkplace.put(currentWid, wid);
                workplaceQueue.get(wid).add(currentWid);
                switchMutex.release();
                try {
                    move.get(currentWid).acquire();
                    switchMutex.acquire();
                } catch (InterruptedException e) {
                    throw new RuntimeException("panic: unexpected thread interruption");
                }
                nextWorkplace.remove(currentWid);
            }
            switchMutex.release();

            if (!cycle.remove(currentWid)) {
                allowEnterWorkplace(currentWid);
            }
        }

        return goToWorkplace(wid);
    }

    @Override
    public void leave() {
        Long threadId = Thread.currentThread().getId();
        WorkplaceId wid = currentWorkplace.get(threadId);
        allowEnterWorkplace(wid);
        previousWorkplace.remove(threadId);
        currentWorkplace.remove(threadId);
        work.get(wid).release();
    }

    public void allowEnterQueue() {
        Long threadId = Thread.currentThread().getId();
        synchronized (actions) {
            actionFinished.compute(threadId, (k, v) -> v == null ? 1 : v + 1);
            if (Math.abs(actions.element()) != threadId) {
                return;
            }

            int finishedEnters = actions.element() < 0 ? -1 : 0;
            while (!actions.isEmpty() &&
                    actionFinished.containsKey(Math.abs(actions.element()))) {
                if (actions.element() < 0) {
                    ++finishedEnters;
                }
                actionFinished.compute(Math.abs(actions.element()),
                                        (k, v) -> v > 1 ? v - 1 : null);
                actions.remove();
            }
            if (!actions.isEmpty() && actions.element() < 0) {
                ++finishedEnters;
            }

            enterCount.addAndGet(-finishedEnters);
            while (!allowEnter.isEmpty() && finishedEnters > 0) {
                allowEnter.poll().release();
                --finishedEnters;
            }
        }
    }

    public void allowWorkingOnPrevious() {
        Long threadId = Thread.currentThread().getId();
        WorkplaceId wid = previousWorkplace.get(threadId);
        if (wid != null) {
            work.get(wid).release();
        }
    }

    public void waitForWork(WorkplaceId wid) {
        try {
            work.get(wid).acquire();
        } catch (InterruptedException e) {
            throw new RuntimeException("panic: unexpected thread interruption");
        }
    }

    private void allowEnterWorkplace(WorkplaceId wid) {
        try {
            switchMutex.acquire();
        } catch (InterruptedException e) {
            throw new RuntimeException("panic: unexpected thread interruption");
        }

        Queue<WorkplaceId> q = workplaceQueue.get(wid);
        while (!q.isEmpty() &&
                (!nextWorkplace.containsKey(q.element()) ||
                 nextWorkplace.get(q.element()).compareTo(wid) != 0)) {
            q.remove();
        }
        if (q.isEmpty()) {
            enter.get(wid).release();
        } else {
            move.get(q.element()).release();
            q.remove();
        }

        switchMutex.release();
    }

    private Workplace goToWorkplace(WorkplaceId wid) {
        Long threadId = Thread.currentThread().getId();
        WorkplaceId current = currentWorkplace.put(threadId, wid);
        if (current != null) {
            previousWorkplace.put(threadId, current);
        }
        return workplaces.get(wid);
    }

    private Boolean detectCycle(WorkplaceId start, WorkplaceId end) {
        WorkplaceId current = start;
        while (current != null && current.compareTo(end) != 0) {
            current = nextWorkplace.get(current);
        }
        return current != null;
    }

}
