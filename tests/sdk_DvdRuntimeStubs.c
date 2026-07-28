/*
 * DVD host tests do not create a renderer. VI still references these hooks
 * through the shared OS initialization graph, so provide inert test-runtime
 * implementations.
 */
void SIM_VIInit(void)
{
}

void SIM_Render(void)
{
}

void __GXHostServiceFifoBreakpoint(void)
{
}
