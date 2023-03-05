package cp2022.solution;

import java.util.Collection;

import cp2022.base.Workplace;
import cp2022.base.Workshop;


public final class WorkshopFactory {

    public final static Workshop newWorkshop(Collection<Workplace> workplaces) {
        return new ConcurrentWorkshop(workplaces);
    }

}
