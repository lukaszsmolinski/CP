package cp2022.solution;

import cp2022.base.Workplace;


public class ConcurrentWorkplace extends Workplace {

    private final Workplace workplace;
    private final ConcurrentWorkshop workshop;

    public ConcurrentWorkplace(Workplace workplace, ConcurrentWorkshop workshop) {
        super(workplace.getId());
        this.workplace = workplace;
        this.workshop = workshop;
    }

    @Override
    public void use() {
        workshop.allowWorkingOnPrevious();
        workshop.allowEnterQueue();
        workshop.waitForWork(getId());
        workplace.use();
    }

}
