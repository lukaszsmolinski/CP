package cp2022.base;


public abstract class Workplace {

    private final WorkplaceId id;

    protected Workplace(WorkplaceId id) {
        this.id = id;
    }

    public final WorkplaceId getId() {
        return this.id;
    }

    public abstract void use();
}
