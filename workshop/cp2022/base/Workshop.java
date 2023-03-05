package cp2022.base;


public interface Workshop {

    public Workplace enter(WorkplaceId wid);
    public Workplace switchTo(WorkplaceId wid);
    public void leave();

}
