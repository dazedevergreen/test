project(org_mitk_lancet_dentalwidget)

mitk_create_plugin(
  EXPORT_DIRECTIVE DENTALWIDGET_EXPORT
  EXPORTED_INCLUDE_SUFFIXES src
  MODULE_DEPENDS MitkQtWidgetsExt MitkBoundingShape MitkLancetAlgo MitkLancetGeoUtil MitkDICOM   MitkLancetRegistration MitkMatchPointRegistration MitkCore
)
